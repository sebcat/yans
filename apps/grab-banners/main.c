#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include <lib/net/dsts.h>
#include <lib/net/sconn.h>
#include <lib/net/reaplan.h>

#define DFL_NCLIENTS    16

#define RECVBUF_SIZE   8192

struct opts {
  int nclients;
  char **dsts;
  int ndsts;
};

struct banner_grabber {
  struct dsts_ctx dsts;
  size_t nclients;
  size_t active_clients;
  int *clients;
  char *recvbuf;
};

#define banner_grabber_get_recvbuf(b_) (b_)->recvbuf

static int banner_grabber_init(struct banner_grabber *b, size_t nclients) {
  int *clients;
  char *recvbuf;
  size_t i;
  int ret;
  struct dsts_ctx dsts;

  clients = malloc(nclients * sizeof(int));
  if (clients == NULL) {
    goto fail;
  }

  recvbuf = malloc(RECVBUF_SIZE * sizeof(char));
  if (recvbuf == NULL) {
    goto fail_free_clients;
  }

  ret = dsts_init(&dsts);
  if (ret < 0) {
    goto fail_free_recvbuf;
  }

  b->dsts = dsts;
  b->nclients = nclients;
  b->active_clients = 0;
  b->clients = clients;
  b->recvbuf = recvbuf;
  for (i = 0; i < nclients; i++) {
    b->clients[i] = -1;
  }

  return 0;

fail_free_recvbuf:
  free(recvbuf);
fail_free_clients:
  free(clients);
fail:
  return -1;
}

static void banner_grabber_cleanup(struct banner_grabber *b) {
  free(b->clients);
  free(b->recvbuf);
  dsts_cleanup(&b->dsts);
  b->nclients = 0;
  b->clients = NULL;
  b->recvbuf = NULL;
}

static int banner_grabber_add_dsts(struct banner_grabber *b,
    const char *addrs, const char *ports) {
  int ret;

  ret = dsts_add(&b->dsts, addrs, ports, NULL);
  if (ret < 0) {
    return -1;
  }

  return 0;
}

static int on_connect(struct reaplan_ctx *ctx, struct reaplan_conn *conn) {
  struct sconn_ctx sconn = {0};
  struct sconn_opts sopts = {0};
  struct banner_grabber *grabber;
  int fd = -1;
  union {
    struct sockaddr sa;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } addr;
  socklen_t addrlen;

  grabber = reaplan_get_data(ctx);
  while (dsts_next(&grabber->dsts, &addr.sa, &addrlen, NULL)) {
    sopts.proto = IPPROTO_TCP;
    sopts.dstaddr = &addr.sa;
    sopts.dstaddrlen = addrlen;
    fd = sconn_connect(&sconn, &sopts);
    if (fd < 0) {
      /* TODO: connect_failure callback? */
      fprintf(stderr, "connection failed: %s\n", strerror(sconn_errno(&sconn)));
      continue;
    }

    conn->fd = fd;
    conn->events = REAPLAN_READABLE | REAPLAN_WRITABLE_ONESHOT;
    return REAPLANC_OK;
  }

  return REAPLANC_DONE;
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
  fprintf(stderr, "done fd:%d err:%d\n", fd, err);
}

static void opts_or_die(struct opts *opts, int argc, char *argv[]) {
  int ch;
  const char *optstring = "n:";
  const char *argv0;
  struct option optcfgs[] = {
    {"nclients",     required_argument, NULL, 'n'},
    {NULL, 0, NULL, 0}
  };

  argv0 = argv[0];

  /* fill in defaults */
  opts->nclients = DFL_NCLIENTS;

  /* override defaults with command line arguments */
  while ((ch = getopt_long(argc, argv, optstring, optcfgs, NULL)) != -1) {
    switch(ch) {
    case 'n':
      opts->nclients = strtol(optarg, NULL, 10);
      break;
    default:
      goto usage;
    }
  }

  argc -= optind;
  argv += optind;
  if (argc <= 0) {
    fprintf(stderr, "missing host/port pairs\n");
    goto usage;
  } else if (argc & 1) {
    fprintf(stderr, "uneven number of host/port elements\n");
    goto usage;
  }
  opts->dsts = argv;
  opts->ndsts = argc;

  return;
usage:
  fprintf(stderr, "%s [opts] <hosts0> <ports0> .. [hostsN] [portsN]\n"
      "opts:\n"
      "  -n|--nclients <n>         number of clients (%d)\n",
      argv0, DFL_NCLIENTS);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  int i;
  int ret;
  struct opts opts;
  struct reaplan_ctx reaplan;
  struct banner_grabber grabber;
  int status = EXIT_FAILURE;
  struct reaplan_opts rpopts = {
    .funcs = {
      .on_connect  = on_connect,
      .on_readable = on_readable,
      .on_writable = on_writable,
      .on_done     = on_done,
    },
    .data = NULL,
  };

  /* parse command line arguments to option struct */
  opts_or_die(&opts, argc, argv);

  /* initialize the banner grabber */
  if (banner_grabber_init(&grabber, opts.nclients) < 0) {
    perror("banner_grabber_init");
    goto done;
  }

  /* add the destinations to the banner grabber */
  for (i = 0; i < opts.ndsts / 2; i++) {
    ret = banner_grabber_add_dsts(&grabber, opts.dsts[i*2],
        opts.dsts[i*2+1]);
    if (ret != 0) {
      fprintf(stderr, "banner_grabber_add_dsts: failed to add: %s %s\n",
          opts.dsts[i*2], opts.dsts[i*2+1]);
      goto done_banner_grabber_cleanup;
    }
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

  return status;
}
