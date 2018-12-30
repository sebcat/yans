#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>

#include <netdb.h>

#include <lib/net/dsts.h>
#include <lib/net/reaplan.h>
#include <lib/net/tcpsrc.h>
#include <lib/util/io.h>
#include <lib/util/sandbox.h>
#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>

#define DFL_NCLIENTS    16 /* maxumum number of concurrent connections */
#define DFL_TIMEOUT      9 /* maximum connection lifetime, in seconds */
#define DFL_CONNECTS_PER_TICK 8
#define DFL_MDELAY_PER_TICK 500

#define RECVBUF_SIZE   8192

/* banner grabber options */
struct bgrab_opts {
  int max_clients;
  int timeout;
  int connects_per_tick;
  int mdelay_per_tick;
  void (*on_error)(const char *);
};

/* banner grabber context */
struct bgrab_ctx {
  struct bgrab_opts opts;
  struct tcpsrc_ctx tcpsrc;
  struct ycl_msg msgbuf;
  struct dsts_ctx dsts;
  char *recvbuf;
};

/* command line options */
struct opts {
  char **dsts;
  int ndsts;
  int no_sandbox;
  struct bgrab_opts bgrab;
};

#define bgrab_get_recvbuf(b_) (b_)->recvbuf

static int bgrab_init(struct bgrab_ctx *b, struct bgrab_opts *opts,
    struct tcpsrc_ctx tcpsrc) {
  char *recvbuf;
  int ret;
  struct dsts_ctx dsts;
  struct ycl_msg msgbuf;

  if (opts->max_clients <= 0) {
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

  ret = ycl_msg_init(&msgbuf);
  if (ret != YCL_OK) {
    goto fail_dsts_cleanup;
  }

  b->tcpsrc       = tcpsrc;
  b->dsts         = dsts;
  b->msgbuf       = msgbuf;
  b->recvbuf      = recvbuf;
  b->opts         = *opts;
  return 0;
fail_dsts_cleanup:
  dsts_cleanup(&dsts);
fail_free_recvbuf:
  free(recvbuf);
fail:
  return -1;
}

static void bgrab_cleanup(struct bgrab_ctx *b) {
  free(b->recvbuf);
  dsts_cleanup(&b->dsts);
  b->recvbuf = NULL;
}

static int bgrab_add_dsts(struct bgrab_ctx *b,
    const char *addrs, const char *ports, void *udata) {
  int ret;

  ret = dsts_add(&b->dsts, addrs, ports, udata);
  if (ret < 0) {
    return -1;
  }

  return 0;
}

static void on_done(struct reaplan_ctx *ctx, struct reaplan_conn *conn) {
  struct bgrab_ctx *grabber;

  grabber = reaplan_get_udata(ctx);
}

static int on_connect(struct reaplan_ctx *ctx, struct reaplan_conn *conn) {
  struct bgrab_ctx *grabber;
  int fd = -1;
  union {
    struct sockaddr sa;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } addr;
  socklen_t addrlen;

  grabber = reaplan_get_udata(ctx);
  while (dsts_next(&grabber->dsts, &addr.sa, &addrlen, NULL)) {
    fd = tcpsrc_connect(&grabber->tcpsrc, &addr.sa);
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

#define handle_errorf(ctx, ...)         \
  if (ctx->opts.on_error != NULL) {     \
    _handle_errorf(ctx, __VA_ARGS__);   \
  }

static void _handle_errorf(struct bgrab_ctx *ctx, const char *fmt, ...) {
  va_list ap;
  char msgbuf[128];

  va_start(ap, fmt);
  vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
  ctx->opts.on_error(msgbuf);
  va_end(ap);
}

static int socket_peername(int sock, char *addr, size_t addrlen,
    char *serv, size_t servlen) {
  int ret;
  socklen_t slen;
  union {
    struct sockaddr sa;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } u;

  slen = sizeof(u);
  ret = getpeername(sock, &u.sa, &slen);
  if (ret != 0) {
    return -1;
  }

  ret = getnameinfo(&u.sa, slen, addr, addrlen, serv, servlen,
      NI_NUMERICHOST | NI_NUMERICSERV);
  if (ret != 0) {
    return -1;
  }

  return 0;
}

static void write_banner(struct bgrab_ctx *ctx, int sockfd,
    const char *banner, size_t bannerlen) {
  struct ycl_msg_banner bannermsg = {{0}};
  char addrstr[128];
  char servstr[32];
  int ret;
  io_t io;

  ret = socket_peername(sockfd, addrstr, sizeof(addrstr), servstr,
      sizeof(servstr));
  if (ret < 0) {
    handle_errorf(ctx, "write_banner: failed to get peername (fd:%d)",
        sockfd);
    return;
  }

  ycl_msg_reset(&ctx->msgbuf);

  /* TODO: fill in "name" field */
  bannermsg.host.data = addrstr;
  bannermsg.host.len = strlen(addrstr);
  bannermsg.port.data = servstr;
  bannermsg.port.len = strlen(servstr);
  bannermsg.banner.data = banner;
  bannermsg.banner.len = bannerlen;
  ret = ycl_msg_create_banner(&ctx->msgbuf, &bannermsg);
  if (ret != YCL_OK) {
    handle_errorf(ctx,
        "write_banner: failed fo serialize %zu bytes banner for %s:%s",
        bannerlen, addrstr, servstr);
    goto ycl_msg_cleanup;
  }

  IO_INIT(&io, STDOUT_FILENO); /* TODO: use different fd */
  ret = io_writeall(&io, ycl_msg_bytes(&ctx->msgbuf),
      ycl_msg_nbytes(&ctx->msgbuf));
  if (ret != IO_OK) {
    handle_errorf(ctx,
        "write_banner: failed to write %zu butes banner for %s:%s - %s",
        bannerlen, addrstr, servstr, io_strerror(&io));
    goto ycl_msg_cleanup;
  }

ycl_msg_cleanup:
  ycl_msg_cleanup(&ctx->msgbuf);
}

static ssize_t on_readable(struct reaplan_ctx *ctx,
    struct reaplan_conn *conn) {
  ssize_t nread;
  struct bgrab_ctx *grabber;
  char *recvbuf;
  int fd;

  grabber = reaplan_get_udata(ctx);
  fd = reaplan_conn_get_fd(conn);
  recvbuf = bgrab_get_recvbuf(grabber);

  nread = read(fd, recvbuf, RECVBUF_SIZE);
  if (nread > 0) {
    write_banner(grabber, fd, recvbuf, nread);
    return 0; /* signal done to reaplan - will close the fd */
  }

  return nread;
}

static ssize_t on_writable(struct reaplan_ctx *ctx,
    struct reaplan_conn *conn) {
#define SrTR "GET / HTTP/1.0\r\n\r\n"
  return write(reaplan_conn_get_fd(conn), SrTR, sizeof(SrTR)-1);
#undef SrTR
}

static int bgrab_run(struct bgrab_ctx *grabber) {
  int ret;
  struct reaplan_ctx reaplan;
  struct reaplan_opts rpopts = {
    .on_connect        = on_connect,
    .on_readable       = on_readable,
    .on_writable       = on_writable,
    .on_done           = on_done,
    .udata             = grabber,
    .max_clients       = grabber->opts.max_clients,
    .timeout           = grabber->opts.timeout,
    .connects_per_tick = grabber->opts.connects_per_tick,
    .mdelay_per_tick   = grabber->opts.mdelay_per_tick,
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

static void print_bgrab_error(const char *err) {
  fprintf(stderr, "%s\n", err);
}

static void opts_or_die(struct opts *opts, int argc, char *argv[]) {
  int ch;
  const char *optstring = "n:t:Xc:d:";
  const char *argv0;
  struct option optcfgs[] = {
    {"max-clients",       required_argument, NULL, 'n'},
    {"timeout",           required_argument, NULL, 't'},
    {"no-sandbox",        no_argument,       NULL, 'X'},
    {"connects-per-tick", required_argument, NULL, 'c'},
    {"mdelay-per-tick",   required_argument, NULL, 'd'},
    {NULL, 0, NULL, 0}
  };

  argv0 = argv[0];

  /* fill in defaults */
  opts->bgrab.max_clients = DFL_NCLIENTS;
  opts->bgrab.timeout = DFL_TIMEOUT;
  opts->bgrab.connects_per_tick = DFL_CONNECTS_PER_TICK;
  opts->bgrab.mdelay_per_tick = DFL_MDELAY_PER_TICK;
  opts->bgrab.on_error = print_bgrab_error;

  /* override defaults with command line arguments */
  while ((ch = getopt_long(argc, argv, optstring, optcfgs, NULL)) != -1) {
    switch(ch) {
    case 'n':
      opts->bgrab.max_clients = strtol(optarg, NULL, 10);
      break;
    case 't':
      opts->bgrab.timeout = strtol(optarg, NULL, 10);
      break;
    case 'X':
      opts->no_sandbox = 1;
      break;
    case 'c':
      opts->bgrab.connects_per_tick = strtol(optarg, NULL, 10);
      break;
    case 'd':
      opts->bgrab.mdelay_per_tick = strtol(optarg, NULL, 10);
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

  if (opts->bgrab.max_clients <= 0) {
    fprintf(stderr, "unvalid max-clients value\n");
    goto usage;
  } else if (opts->bgrab.connects_per_tick <= 0) {
    fprintf(stderr, "connects-per-tick set too low\n");
    goto usage;
  } else if (opts->bgrab.connects_per_tick > opts->bgrab.max_clients) {
    fprintf(stderr, "connects-per-tick higher than max-clients\n");
    goto usage;
  }

  return;
usage:
  fprintf(stderr, "%s [opts] <hosts0> <ports0> .. [hostsN] [portsN]\n"
      "opts:\n"
      "  -n|--max-clients       <n> # of max concurrent clients (%d)\n"
      "  -t|--timeout           <n> connection lifetime in seconds (%d)\n"
      "  -X|--no-sandbox            disable sandbox\n"
      "  -c|--connects-per-tick <n> # of conns per discretization (%d)\n"
      "  -d|--mdelay-per-tick   <n> millisecond delay per tick (%d)\n",
      argv0, DFL_NCLIENTS, DFL_TIMEOUT, DFL_CONNECTS_PER_TICK,
      DFL_MDELAY_PER_TICK);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  int i;
  int ret;
  struct opts opts = {0};
  struct bgrab_ctx grabber;
  int status = EXIT_FAILURE;
  struct tcpsrc_ctx tcpsrc;

  /* parse command line arguments to option struct */
  opts_or_die(&opts, argc, argv);

  /* ignore SIGPIPE, caused by writes on a file descriptor where the peer
   * has closed the connection */
  signal(SIGPIPE, SIG_IGN);

  /* initialice the TCP client connection source */
  ret = tcpsrc_init(&tcpsrc);
  if (ret != 0) {
    perror("tcpsrc_init");
    goto done;
  }

  /* enter sandbox mode unless sandbox is disabled */
  if (opts.no_sandbox) {
    fprintf(stderr, "warning: sandbox disabled\n");
  } else {
    ret = sandbox_enter();
    if (ret != 0) {
      fprintf(stderr, "failed to enter sandbox mode\n");
      goto done_tcpsrc_cleanup;
    }
  }

  /* initialize the banner grabber */
  if (bgrab_init(&grabber, &opts.bgrab, tcpsrc) < 0) {
    perror("bgrab_init");
    goto done_tcpsrc_cleanup;
  }

  /* add the destinations to the banner grabber */
  for (i = 0; i < opts.ndsts / 2; i++) {
    ret = bgrab_add_dsts(&grabber, opts.dsts[i*2],
        opts.dsts[i*2+1], NULL);
    if (ret != 0) {
      fprintf(stderr, "bgrab_add_dsts: failed to add: %s %s\n",
          opts.dsts[i*2], opts.dsts[i*2+1]);
      goto done_bgrab_cleanup;
    }
  }

  /* Grab all the banners! */
  ret = bgrab_run(&grabber);
  if (ret < 0) {
    fprintf(stderr, "bgrab_run: failure (%s)\n", strerror(errno));
    goto done_bgrab_cleanup;
  }

  status = EXIT_SUCCESS;
done_bgrab_cleanup:
  bgrab_cleanup(&grabber);
done_tcpsrc_cleanup:
  tcpsrc_cleanup(&tcpsrc);
done:
  return status;
}
