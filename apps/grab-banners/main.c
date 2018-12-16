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
  int max_clients;
  char **dsts;
  int ndsts;
};

struct banner_grabber {
  struct connector_ctx *connector;
  struct dsts_ctx dsts;
  char *recvbuf;

  int curr_clients;
  int max_clients;
};

#define banner_grabber_get_recvbuf(b_) (b_)->recvbuf

static int banner_grabber_init(struct banner_grabber *b,
    struct connector_ctx *connector, int max_clients) {
  char *recvbuf;
  int ret;
  struct dsts_ctx dsts;

  if (max_clients <= 0) {
    goto fail;
  }

  memset(b, 0, sizeof(*b));

  recvbuf = malloc(RECVBUF_SIZE * sizeof(char));
  if (recvbuf == NULL) {
    goto fail;
  }

  ret = dsts_init(&dsts);
  if (ret < 0) {
    goto fail_free_recvbuf;
  }

  b->connector    = connector;
  b->dsts         = dsts;
  b->recvbuf      = recvbuf;
  b->curr_clients = 0;
  b->max_clients  = max_clients;

  return 0;

fail_free_recvbuf:
  free(recvbuf);
fail:
  return -1;
}

static void banner_grabber_cleanup(struct banner_grabber *b) {
  free(b->recvbuf);
  dsts_cleanup(&b->dsts);
  b->recvbuf = NULL;
}

static int banner_grabber_add_dsts(struct banner_grabber *b,
    const char *addrs, const char *ports, void *udata) {
  int ret;

  ret = dsts_add(&b->dsts, addrs, ports, udata);
  if (ret < 0) {
    return -1;
  }

  return 0;
}

static void on_done(struct reaplan_ctx *ctx, struct reaplan_conn *conn) {
  struct banner_grabber *grabber;

  grabber = reaplan_get_udata(ctx);
  grabber->curr_clients--;
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

  grabber = reaplan_get_udata(ctx);
  if (grabber->curr_clients >= grabber->max_clients) {
    return REAPLANC_WAIT;
  }

  while (dsts_next(&grabber->dsts, &addr.sa, &addrlen, NULL)) {
    /* increment the client counter early, so that if the connection fail
     * the count is corrected by on_done either way */
    grabber->curr_clients++;

    sopts.proto = IPPROTO_TCP;
    sopts.dstaddr = &addr.sa;
    sopts.dstaddrlen = addrlen;
    fd = connector_connect(grabber->connector, &sopts);
    if (fd < 0) {
      on_done(ctx, conn);
      continue;
    }

    reaplan_conn_register(conn, fd,
        REAPLAN_READABLE | REAPLAN_WRITABLE_ONESHOT);
    return REAPLANC_OK;
  }

  return REAPLANC_DONE;
}

static ssize_t on_readable(struct reaplan_ctx *ctx,
    struct reaplan_conn *conn) {
  ssize_t nread;
  struct banner_grabber *grabber;
  char *recvbuf;
  int fd;

  grabber = reaplan_get_udata(ctx);
  fd = reaplan_conn_get_fd(conn);
  recvbuf = banner_grabber_get_recvbuf(grabber);

  nread = read(fd, recvbuf, RECVBUF_SIZE);
  if (nread > 0) {
    /* TODO: Do something with the data other than writing it */
    write(STDOUT_FILENO, recvbuf, nread);
    return 0; /* signal EOF to reaplan */
  }

  return nread;
}

static ssize_t on_writable(struct reaplan_ctx *ctx,
    struct reaplan_conn *conn) {
#define SrTR "GET / HTTP/1.0\r\n\r\n"
  return write(reaplan_conn_get_fd(conn), SrTR, sizeof(SrTR)-1);
#undef SrTR
}

static int banner_grabber_run(struct banner_grabber *grabber) {
  int ret;
  struct reaplan_ctx reaplan;
  struct reaplan_opts rpopts = {
    .on_connect  = on_connect,
    .on_readable = on_readable,
    .on_writable = on_writable,
    .on_done     = on_done,
    .max_clients = grabber->max_clients,
    .udata       = grabber,
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
  const char *optstring = "n:s:";
  const char *argv0;
  struct option optcfgs[] = {
    {"max-clients",     required_argument, NULL, 'n'},
    {"socket",    required_argument, NULL, 's'},
    {NULL, 0, NULL, 0}
  };

  argv0 = argv[0];

  /* fill in defaults */
  opts->max_clients = DFL_NCLIENTS;
  opts->connector_path = NULL;

  /* override defaults with command line arguments */
  while ((ch = getopt_long(argc, argv, optstring, optcfgs, NULL)) != -1) {
    switch(ch) {
    case 'n':
      opts->max_clients = strtol(optarg, NULL, 10);
      break;
    case 's':
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

  if (opts->max_clients <= 0) {
    fprintf(stderr, "unvalid max-client value\n");
    goto usage;
  }

  return;
usage:
  fprintf(stderr, "%s [opts] <hosts0> <ports0> .. [hostsN] [portsN]\n"
      "opts:\n"
      "  -s|--socket       <path>  path to connector service socket\n"
      "  -n|--max-clients  <n>     # of max concurrent clients (%d)\n",
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

  /* ignore SIGPIPE, caused by writes on a file descriptor where the peer
   * has closed the connection */
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
  if (banner_grabber_init(&grabber, &connector, opts.max_clients) < 0) {
    perror("banner_grabber_init");
    goto done_connector_cleanup;
  }

  /* add the destinations to the banner grabber */
  for (i = 0; i < opts.ndsts / 2; i++) {
    ret = banner_grabber_add_dsts(&grabber, opts.dsts[i*2],
        opts.dsts[i*2+1], NULL);
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
