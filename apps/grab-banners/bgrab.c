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

#define RECVBUF_SIZE         8192
#define INITIAL_CERTBUF_SIZE 8192

struct bgrab_dst {
  union {
    struct sockaddr sa;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } addr;
  socklen_t addrlen;
  const char *name;
};

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

  buf_init(&ctx->certbuf, INITIAL_CERTBUF_SIZE);
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
  if (ctx) {
    free(ctx->recvbuf);
    ctx->recvbuf = NULL;
    dsts_cleanup(&ctx->dsts);
    buf_cleanup(&ctx->certbuf);
  }
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

static int bgrab_next_dst(struct bgrab_ctx *ctx, struct bgrab_dst **out) {
  void *name = NULL;
  struct bgrab_dst *dst;
  int ret;

  /* TODO: Keep a bgrab_dst object pool */
  dst = malloc(sizeof(*dst));
  if (dst == NULL) {
    return 0;
  }

  ret = dsts_next(&ctx->dsts, &dst->addr.sa, &dst->addrlen, &name);
  if (ret != 1) {
    free(dst);
    return 0;
  }

  dst->name = name;
  *out = dst;
  return 1;
}

static void write_banner(struct bgrab_ctx *ctx, struct reaplan_conn *conn,
    const char *banner, size_t bannerlen) {
  struct ycl_msg_banner bannermsg = {{0}};
  struct bgrab_dst *dst;
  char addrstr[128];
  char servstr[32];
  int ret;
  size_t sz;

  dst = reaplan_conn_get_udata(conn);
  ret = getnameinfo(&dst->addr.sa, dst->addrlen, addrstr, sizeof(addrstr),
      servstr, sizeof(servstr), NI_NUMERICHOST | NI_NUMERICSERV);
  if (ret != 0) {
    handle_errorf(ctx, "write_banner: getnameinfo: %s", gai_strerror(ret));
    return;
  }

  ycl_msg_reset(&ctx->msgbuf);
  buf_clear(&ctx->certbuf);
  reaplan_conn_append_cert_chain(conn, &ctx->certbuf);
  bannermsg.name.data = dst->name;
  bannermsg.name.len = dst->name ? strlen(dst->name) : 0;
  bannermsg.certs.data = ctx->certbuf.data;
  bannermsg.certs.len = ctx->certbuf.len;
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

static void on_done(struct reaplan_ctx *reaplan,
    struct reaplan_conn *conn) {
  struct bgrab_dst *dst;
  unsigned int nwritten;
  unsigned int nread;
  struct bgrab_ctx *ctx;

  nwritten = reaplan_conn_get_nwritten(conn);
  nread = reaplan_conn_get_nread(conn);
  if (nwritten > 0 && nread == 0) {
    /* If we have written more than zero bytes, but received zero bytes
     * we should still call write_banner here to indicate that we have
     * had an established connection */
    ctx = reaplan_get_udata(reaplan);
    write_banner(ctx, conn, NULL, 0);
  }

  dst = reaplan_conn_get_udata(conn);
  free(dst);
}

static int on_connect(struct reaplan_ctx *reaplan,
    struct reaplan_conn *conn) {
  struct bgrab_ctx *ctx;
  struct bgrab_dst *dst;
  int fd = -1;
  int ret;

  ctx = reaplan_get_udata(reaplan);
  while (bgrab_next_dst(ctx, &dst)) {
    reaplan_conn_set_udata(conn, dst);
    fd = tcpsrc_connect(&ctx->tcpsrc, &dst->addr.sa);
    if (fd < 0) {
      on_done(reaplan, conn);
      continue;
    }

    ret = reaplan_register_conn(reaplan, conn, fd,
        REAPLAN_READABLE | REAPLAN_WRITABLE_ONESHOT, dst->name);
    if (ret < 0) {
      on_done(reaplan, conn);
      continue;
    }

    return REAPLANC_OK;
  }

  return REAPLANC_DONE;
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
  struct payload_host h;
  struct bgrab_dst *dst;
  struct payload_data *payload;
  ssize_t res;

  /* TODO:
   * - support multiple different payloads, not only HTTP req
   * - cache payloads across connections (LRU eviction?)
   * - currently, it is assumed that the entire payload is sent with
   *   one call to reaplan_conn_write which is often the case but it
   *   is not a guarantee. It would be better to store the payload and
   *   a written offset as connection udata and make this func reentrant.
   */
  dst = reaplan_conn_get_udata(conn);
  h.salen = dst->addrlen;
  h.sa = &dst->addr.sa;
  h.name = dst->name;
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

  /* The destinations are reset for every invocation of bgrab_run */
  dsts_cleanup(&ctx->dsts);
  dsts_init(&ctx->dsts);

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
