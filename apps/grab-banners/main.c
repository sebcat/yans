#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include <lib/net/sconn.h>
#include <lib/net/reaplan.h>

#define DFL_SUBJECTFILE "-"
#define DFL_PORTS       "80,443"
#define DFL_NCLIENTS    16

#define RECVBUF_SIZE   8192

struct opts {
  const char *subjectfile;
  const char *ports;
  int nclients;
};

struct banner_grabber {
  FILE *src;
  size_t nclients;
  size_t active_clients;
  int *clients;
  char *recvbuf;
};

#define banner_grabber_get_src(b_) (b_)->src
#define banner_grabber_get_recvbuf(b_) (b_)->recvbuf

static int banner_grabber_init(struct banner_grabber *b, FILE *src,
    size_t nclients) {
  int *clients;
  char *recvbuf;
  size_t i;

  clients = malloc(nclients * sizeof(int));
  if (clients == NULL) {
    return -1;
  }

  recvbuf = calloc(RECVBUF_SIZE, sizeof(char));
  if (recvbuf == NULL) {
    free(clients);
    return -1;
  }

  b->src = src;
  b->nclients = nclients;
  b->active_clients = 0;
  b->clients = clients;
  b->recvbuf = recvbuf;
  for (i = 0; i < nclients; i++) {
    b->clients[i] = -1;
  }

  return 0;
}

static void banner_grabber_cleanup(struct banner_grabber *b) {
  free(b->clients);
  free(b->recvbuf);
  b->nclients = 0;
  b->clients = NULL;
  b->recvbuf = NULL;
}

static int on_connect(struct reaplan_ctx *ctx, struct reaplan_conn *conn) {
  struct sconn_ctx sconn;
  struct sconn_opts sopts = {
    .proto = "tcp",  /* FIXME: sconn has too many string options  */
    .dstport = "80", /* FIXME: port_range */
  };
  struct banner_grabber *grabber;
  FILE *fp;
  char linebuf[56];
  char addrbuf[56];
  int fd = -1;

  grabber = reaplan_get_data(ctx);
  fp = banner_grabber_get_src(grabber);

  while (fd < 0) {
    /* TODO: Instead of reading one address at a time, read N addresses
     *       to a table and iterate once over the table for each port */
    if (fgets(linebuf, sizeof(linebuf), fp) == NULL) {
      return REAPLANC_DONE;
    }

    if (sscanf(linebuf, "%s", addrbuf) != 1) {
      continue;
    }

    sopts.dstaddr = addrbuf;
    fd = sconn_connect(&sconn, &sopts);
    if (fd < 0) {
      fprintf(stderr, "connection failed: \"%s\" %s\n", addrbuf,
          strerror(sconn_errno(&sconn)));
    }
  }

  conn->fd = fd;
  conn->events = REAPLAN_READABLE | REAPLAN_WRITABLE_ONESHOT;
  return REAPLANC_OK;
}

static ssize_t on_readable(struct reaplan_ctx *ctx, int fd) {
  ssize_t nread;
  struct banner_grabber *grabber;
  char *recvbuf;

  grabber = reaplan_get_data(ctx);
  recvbuf = banner_grabber_get_recvbuf(grabber);

  nread = read(fd, recvbuf, RECVBUF_SIZE);
  if (nread > 0) {
    write(STDOUT_FILENO, recvbuf, nread);
    return 0; /* signal EOF to reaplan */
  }

  return nread;
}

static ssize_t on_writable(struct reaplan_ctx *ctx, int fd) {
#define SrTR "GET / HTTP/1.0\r\n\r\n"
  return write(fd, SrTR, sizeof(SrTR)-1);
#undef SrTR
}

static void on_done(struct reaplan_ctx *ctx, int fd, int err) {
  printf("done fd:%d err:%d\n", fd, err);
}

static void opts_or_die(struct opts *opts, int argc, char *argv[]) {
  int ch;
  const char *optstring = "f:p:n:";
  struct option optcfgs[] = {
    {"subject-file", required_argument, NULL, 'f'},
    {"ports",        required_argument, NULL, 'p'},
    {"nclients",     required_argument, NULL, 'n'},
    {NULL, 0, NULL, 0}
  };

  /* fill in defaults */
  opts->subjectfile = DFL_SUBJECTFILE;
  opts->ports = DFL_PORTS;
  opts->nclients = DFL_NCLIENTS;

  /* override defaults with command line arguments */
  while ((ch = getopt_long(argc, argv, optstring, optcfgs, NULL)) != -1) {
    switch(ch) {
    case 'f':
      opts->subjectfile = optarg;
      break;
    case 'p':
      opts->ports = optarg;
      break;
    case 'n':
      opts->nclients = strtol(optarg, NULL, 10);
    default:
      goto usage;
    }
  }

  return;
usage:
  fprintf(stderr, "usage: %s [opts]\n"
      "opts:\n"
      "  -f|--subject-file <file>  file with <addr>,<name>\\n lines (%s)\n"
      "  -p|--ports <portspec>     ports to connect to (%s)\n"
      "  -n|--nclients <n>         number of clients (%d)\n",
      argv[0], DFL_SUBJECTFILE, DFL_PORTS, DFL_NCLIENTS);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  int ret;
  FILE *src = NULL;
  struct opts opts;
  struct reaplan_ctx reaplan;
  struct banner_grabber grabber;
  int status = EXIT_FAILURE;
  struct reaplan_opts rpopts = {
    .funcs = {
      .on_connect = on_connect,
      .on_readable = on_readable,
      .on_writable = on_writable,
      .on_done = on_done,
    },
    .data = NULL,
  };

  /* parse command line arguments to option struct */
  opts_or_die(&opts, argc, argv);

  /* set/open src file */
  if (opts.subjectfile[0] == '-' && opts.subjectfile[1] == '\0') {
    src = stdin;
  } else {
    src = fopen(opts.subjectfile, "rb");
    if (src == NULL) {
      perror("fopen");
      return EXIT_FAILURE;
    }
  }

  /* initialize the banner grabber */
  if (banner_grabber_init(&grabber, src, opts.nclients) < 0) {
    perror("banner_grabber_init");
    goto done;
  }

  /* initialize the event "engine" */
  rpopts.data = &grabber;
  ret = reaplan_init(&reaplan, &rpopts);
  if (ret != REAPLAN_OK) {
    perror("reaplan_init");
    goto done_banner_grabber_cleanup;
  }

  /* run until all connections are completed */
  ret = reaplan_run(&reaplan);
  if (ret != REAPLAN_OK) {
    perror("reaplan_run");
    goto done_reaplan_cleanup;
  }

  status = EXIT_SUCCESS;
done_reaplan_cleanup:
  reaplan_cleanup(&reaplan);
done_banner_grabber_cleanup:
  banner_grabber_cleanup(&grabber);
done:
  if (src != stdin) {
    fclose(src);
  }

  return status;
}
