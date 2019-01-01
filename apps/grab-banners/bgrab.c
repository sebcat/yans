#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include <lib/net/dsts.h>
#include <lib/net/reaplan.h>
#include <lib/net/tcpsrc.h>
#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>

#include <apps/grab-banners/bgrab.h>
#include <apps/grab-banners/payload.h>

#define RECVBUF_SIZE   8192

int bgrab_init(struct bgrab_ctx *ctx, struct bgrab_opts *opts,
    struct tcpsrc_ctx tcpsrc) {
  char *recvbuf;
  int ret;
  struct dsts_ctx dsts;
  struct ycl_msg msgbuf;

  /* set initial error state and validate options */
  ctx->err = BGRABE_NOERR;
  if (opts->max_clients <= 0) {
    ctx->err = BGRABE_INVAL_MAX_CLIENTS;
  } else if (opts->connects_per_tick <= 0) {
    ctx->err = BGRABE_INVAL_CONNECTS_PER_TICK;
  } else if (opts->connects_per_tick > opts->max_clients) {
    ctx->err = BGRABE_TOOHIGH_CONNECTS_PER_TICK;
  } else if (opts->outfile == NULL) {
    ctx->err = BGRABE_INVAL_OUTFILE;
  }

  /* check for errors reported in option check above */
  if (ctx->err != BGRABE_NOERR) {
    goto fail;
  }

  memset(ctx, 0, sizeof(*ctx));

  recvbuf = malloc(RECVBUF_SIZE * sizeof(char));
  if (recvbuf == NULL) {
    ctx->err = BGRABE_NOMEM;
    goto fail;
  }

  ret = dsts_init(&dsts);
  if (ret < 0) {
    ctx->err = BGRABE_NOMEM;
    goto fail_free_recvbuf;
  }

  ret = ycl_msg_init(&msgbuf);
  if (ret != YCL_OK) {
    ctx->err = BGRABE_NOMEM;
    goto fail_dsts_cleanup;
  }

  ctx->tcpsrc  = tcpsrc;
  ctx->dsts    = dsts;
  ctx->msgbuf  = msgbuf;
  ctx->recvbuf = recvbuf;
  ctx->opts    = *opts;
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
    ctx->err = BGRABE_INVAL_DST;
    return -1;
  }

  return 0;
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

static void on_done(struct reaplan_ctx *reaplan,
    struct reaplan_conn *conn) {
  /* NO-OP */
}

static int on_connect(struct reaplan_ctx *reaplan,
    struct reaplan_conn *conn) {
  struct bgrab_ctx *ctx;
  int fd = -1;
  int ret;
  union {
    struct sockaddr sa;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } addr;
  socklen_t addrlen;
  void *dstarg; /* currently used for hostname */

  ctx = reaplan_get_udata(reaplan);
  while (dsts_next(&ctx->dsts, &addr.sa, &addrlen, &dstarg)) {
    fd = tcpsrc_connect(&ctx->tcpsrc, &addr.sa);
    if (fd < 0) {
      on_done(reaplan, conn);
      continue;
    }

    reaplan_conn_set_udata(conn, dstarg);
    ret = reaplan_register_conn(reaplan, conn, fd,
        REAPLAN_READABLE | REAPLAN_WRITABLE_ONESHOT, dstarg);
    if (ret < 0) {
      on_done(reaplan, conn);
      continue;
    }

    return REAPLANC_OK;
  }

  return REAPLANC_DONE;
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

static void write_banner(struct bgrab_ctx *ctx, struct reaplan_conn *conn,
    const char *banner, size_t bannerlen) {
  struct ycl_msg_banner bannermsg = {{0}};
  char addrstr[128];
  char servstr[32];
  int ret;
  size_t sz;
  int sockfd;
  const char *name;

  sockfd = reaplan_conn_get_fd(conn);
  ret = socket_peername(sockfd, addrstr, sizeof(addrstr), servstr,
      sizeof(servstr));
  if (ret < 0) {
    handle_errorf(ctx, "write_banner: failed to get peername (fd:%d)",
        sockfd);
    return;
  }

  ycl_msg_reset(&ctx->msgbuf);

  name = reaplan_conn_get_udata(conn);
  bannermsg.name.data = name;
  bannermsg.name.len = name ? strlen(name) : 0;
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

  sz = fwrite(ycl_msg_bytes(&ctx->msgbuf), ycl_msg_nbytes(&ctx->msgbuf), 1,
      ctx->opts.outfile);
  if (sz != 1) {
    handle_errorf(ctx,
        "write_banner: failed to write %zu butes banner for %s:%s",
        bannerlen, addrstr, servstr);
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

  ctx = reaplan_get_udata(reaplan);
  recvbuf = bgrab_get_recvbuf(ctx);

  nread = reaplan_conn_read(conn, recvbuf, RECVBUF_SIZE);
  if (nread > 0) {
    write_banner(ctx, conn, recvbuf, nread);
    return 0; /* signal done to reaplan - will close the conn */
  }

  return nread;
}

static ssize_t on_writable(struct reaplan_ctx *ctx,
    struct reaplan_conn *conn) {
  int ret;
  int fd;
  struct payload_host h;
  union {
    struct sockaddr sa;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } u;
  struct payload_data *payload;
  ssize_t res;

  h.salen = sizeof(u);
  h.sa = &u.sa;
  h.name = reaplan_conn_get_udata(conn);
  fd = reaplan_conn_get_fd(conn);
  ret = getpeername(fd, &u.sa, &h.salen);
  if (ret != 0) {
    return -1;
  }

  /* TODO:
   * - support multiple different payloads, not only HTTP req
   * - cache payloads across connections (LRU eviction?)
   * - currently, it is assumed that the entire payload is sent with
   *   one call to reaplan_conn_write which is often the case but it
   *   is not a guarantee. It would be better to store the payload and
   *   a written offset as connection udata and make this func reentrant.
   */
  payload = payload_build(
      "GET / HTTP/1.1\r\n"
      "Host: $H\r\n"
      "Accept: */*\r\n"
      "\r\n",
      &h);
  if (payload == NULL) {
    return -1;
  }

  res = reaplan_conn_write(conn, payload_get_data(payload),
      payload_get_len(payload));
  payload_free(payload);
  return res;
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
    .ssl_ctx           = ctx->opts.ssl_ctx,
  };

  ret = reaplan_init(&reaplan, &rpopts);
  if (ret != REAPLAN_OK) {
    ctx->err = BGRABE_RP_INIT;
    return -1;
  }

  ret = reaplan_run(&reaplan);
  reaplan_cleanup(&reaplan);
  if (ret != REAPLAN_OK) {
    ctx->err = BGRABE_RP_RUN;
    return -1;
  }

  return 0;
}

const char *bgrab_strerror(struct bgrab_ctx *ctx) {
  switch(ctx->err) {
  case BGRABE_NOERR:
    return "no known error";
  case BGRABE_INVAL_MAX_CLIENTS:
    return "invalid max_clients option";
  case BGRABE_INVAL_CONNECTS_PER_TICK:
    return "invalid connects_per_tick option";
  case BGRABE_TOOHIGH_CONNECTS_PER_TICK:
    return "connects_per_tick higher than max_clients";
  case BGRABE_INVAL_OUTFILE:
    return "invalid outfile option";
  case BGRABE_NOMEM:
    return "out of memory";
  case BGRABE_INVAL_DST:
    return "invalid destination";
  case BGRABE_RP_INIT:
    return "failure to initialize reaplan";
  case BGRABE_RP_RUN:
    return "failure to run reaplan";
  default:
    return "unknown error";
  }
}
