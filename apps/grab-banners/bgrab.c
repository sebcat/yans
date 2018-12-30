#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include <netdb.h>

#include <lib/net/dsts.h>
#include <lib/net/reaplan.h>
#include <lib/net/tcpsrc.h>
#include <lib/util/io.h> /* TODO: Remove when not needed */
#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>

#include <apps/grab-banners/bgrab.h>

#define RECVBUF_SIZE   8192

int bgrab_init(struct bgrab_ctx *ctx, struct bgrab_opts *opts,
    struct tcpsrc_ctx tcpsrc) {
  char *recvbuf;
  int ret;
  struct dsts_ctx dsts;
  struct ycl_msg msgbuf;

  if (opts->max_clients <= 0) {
    goto fail;
  }

  memset(ctx, 0, sizeof(*ctx));

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

  ctx->tcpsrc       = tcpsrc;
  ctx->dsts         = dsts;
  ctx->msgbuf       = msgbuf;
  ctx->recvbuf      = recvbuf;
  ctx->opts         = *opts;
  return 0;
fail_dsts_cleanup:
  dsts_cleanup(&dsts);
fail_free_recvbuf:
  free(recvbuf);
fail:
  return -1;
}

void bgrab_cleanup(struct bgrab_ctx *ctx) {
  free(ctx->recvbuf);
  dsts_cleanup(&ctx->dsts);
  ctx->recvbuf = NULL;
}

int bgrab_add_dsts(struct bgrab_ctx *ctx,
    const char *addrs, const char *ports, void *udata) {
  int ret;

  ret = dsts_add(&ctx->dsts, addrs, ports, udata);
  if (ret < 0) {
    return -1;
  }

  return 0;
}

static void on_done(struct reaplan_ctx *reaplan,
    struct reaplan_conn *conn) {
  /* NO-OP */
}

static int on_connect(struct reaplan_ctx *reaplan,
    struct reaplan_conn *conn) {
  struct bgrab_ctx *ctx;
  int fd = -1;
  union {
    struct sockaddr sa;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } addr;
  socklen_t addrlen;

  ctx = reaplan_get_udata(reaplan);
  while (dsts_next(&ctx->dsts, &addr.sa, &addrlen, NULL)) {
    fd = tcpsrc_connect(&ctx->tcpsrc, &addr.sa);
    if (fd < 0) {
      on_done(reaplan, conn);
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

static ssize_t on_readable(struct reaplan_ctx *reaplan,
    struct reaplan_conn *conn) {
  ssize_t nread;
  struct bgrab_ctx *ctx;
  char *recvbuf;
  int fd;

  ctx = reaplan_get_udata(reaplan);
  fd = reaplan_conn_get_fd(conn);
  recvbuf = bgrab_get_recvbuf(ctx);

  nread = read(fd, recvbuf, RECVBUF_SIZE);
  if (nread > 0) {
    write_banner(ctx, fd, recvbuf, nread);
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

int bgrab_run(struct bgrab_ctx *ctx) {
  int ret;
  struct reaplan_ctx reaplan;
  struct reaplan_opts rpopts = {
    .on_connect        = on_connect,
    .on_readable       = on_readable,
    .on_writable       = on_writable,
    .on_done           = on_done,
    .udata             = ctx,
    .max_clients       = ctx->opts.max_clients,
    .timeout           = ctx->opts.timeout,
    .connects_per_tick = ctx->opts.connects_per_tick,
    .mdelay_per_tick   = ctx->opts.mdelay_per_tick,
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

