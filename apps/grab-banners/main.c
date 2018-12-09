#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>

#include <lib/net/dsts.h>
#include <lib/net/reaplan.h>
#include <lib/util/sandbox.h>

#include <lib/go-between/connector.h>

#define DFL_NCLIENTS    16

#define RECVBUF_SIZE   8192

struct opts {
  const char *connector_path;
  int nclients;
  char **dsts;
  int ndsts;
};

struct banner_grabber {
  struct connector_ctx *connector;
  struct dsts_ctx dsts;
  size_t nclients;
  size_t active_clients;
  int *clients;
  char *recvbuf;
};

#define banner_grabber_get_recvbuf(b_) (b_)->recvbuf

static int banner_grabber_init(struct banner_grabber *b,
    struct connector_ctx *connector, size_t nclients) {
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

  b->connector = connector;
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
    fd = connector_connect(grabber->connector, &sopts);
    if (fd < 0) {
      /* TODO: connect_failure callback? */
      fprintf(stderr, "connection failed: %s\n",
          connector_strerror(grabber->connector));
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

static int banner_grabber_run(struct banner_grabber *grabber) {
  int ret;
  struct reaplan_ctx reaplan;
  struct reaplan_opts rpopts = {
    .funcs = {
      .on_connect  = on_connect,
      .on_readable = on_readable,
      .on_writable = on_writable,
      .on_done     = on_done,
    },
    .data = grabber,
  };

  ret = reaplan_init(&reaplan, &rpopts);
  if (ret != REAPLAN_OK) {
    return -1;
  }

  ret = reaplan_run(&reaplan);
  reaplan_cleanup(&reaplan);
  if (ret != REAPLAN_OK) {
    return -1;
  }

  return 0;

}

static void opts_or_die(struct opts *opts, int argc, char *argv[]) {
  int ch;
  const char *optstring = "n:c:";
  const char *argv0;
  struct option optcfgs[] = {
    {"nclients",     required_argument, NULL, 'n'},
    {"connector",    required_argument, NULL, 'c'},
    {NULL, 0, NULL, 0}
  };

  argv0 = argv[0];

  /* fill in defaults */
  opts->nclients = DFL_NCLIENTS;
  opts->connector_path = NULL;

  /* override defaults with command line arguments */
  while ((ch = getopt_long(argc, argv, optstring, optcfgs, NULL)) != -1) {
    switch(ch) {
    case 'n':
      opts->nclients = strtol(optarg, NULL, 10);
      break;
    case 'c':
      opts->connector_path = optarg;
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
      "  -c|--connector <path>     path to connector service\n"
      "  -n|--nclients  <n>        number of clients (%d)\n",
      argv0, DFL_NCLIENTS);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  int i;
  int ret;
  struct opts opts;
  struct banner_grabber grabber;
  struct connector_opts copts = {0};
  struct connector_ctx connector;
  struct ycl_msg msgbuf;
  int status = EXIT_FAILURE;
  int do_sandbox = 0;

  /* parse command line arguments to option struct */
  opts_or_die(&opts, argc, argv);

  signal(SIGPIPE, SIG_IGN);

  /* initialize the connector go-between, which handles connections for
   * cases where we're using an external service for connections or not.
   * If we use an external service (typically clid) we enter sandbox mode
   * later. */
  if (opts.connector_path && *opts.connector_path) {
    ret = ycl_msg_init(&msgbuf);
    if (ret != YCL_OK) {
      fprintf(stderr, "failed to initialize YCL message buffer\n");
      goto done;
    }
    copts.svcpath = opts.connector_path;
    copts.msgbuf = &msgbuf;
    do_sandbox = 1;
  }
  ret = connector_init(&connector, &copts);
  if (ret < 0) {
    if (copts.msgbuf != NULL) {
      ycl_msg_cleanup(copts.msgbuf);
    }
    fprintf(stderr, "failed to initialize connector go-between\n");
    goto done;
  }

  if (do_sandbox) {
    ret = sandbox_enter();
    if (ret != 0) {
      fprintf(stderr, "failed to enter sandbox mode\n");
      goto done_connector_cleanup;
    }
  }

  /* initialize the banner grabber */
  if (banner_grabber_init(&grabber, &connector, opts.nclients) < 0) {
    perror("banner_grabber_init");
    goto done_connector_cleanup;
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

  ret = banner_grabber_run(&grabber);
  if (ret < 0) {
    fprintf(stderr, "banner_grabber_run: failure (%s)\n", strerror(errno));
    goto done_banner_grabber_cleanup;
  }

  status = EXIT_SUCCESS;
done_banner_grabber_cleanup:
  banner_grabber_cleanup(&grabber);
done_connector_cleanup:
  if (copts.msgbuf) {
    ycl_msg_cleanup(copts.msgbuf);
  }
  connector_cleanup(&connector);
done:
  return status;
}
