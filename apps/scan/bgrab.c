#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include <lib/util/sha1.h>
#include <lib/net/reaplan.h>
#include <lib/ycl/ycl_msg.h>

#include <apps/scan/bgrab.h>

#define RECVBUF_SIZE         8192
#define INITIAL_CERTBUF_SIZE 8192
#define INITIAL_PAYLOAD_BUFSZ 256

struct payload_data {
  buf_t buf;
};

struct payload_host {
  struct sockaddr *sa;
  socklen_t salen;
  const char *name;
};

struct bgrab_dst {
  union {
    struct sockaddr sa;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } addr;
  socklen_t addrlen;
  const char *name;
};

static inline void *payload_get_data(struct payload_data *payload) {
  return payload->buf.data;
}

static inline size_t payload_get_len(struct payload_data *payload) {
  return payload->buf.len;
}

static struct payload_data *payload_alloc() {
  struct payload_data *payload;

  payload = malloc(sizeof(*payload));
  if (payload == NULL) {
    goto fail;
  }

  if (!buf_init(&payload->buf, INITIAL_PAYLOAD_BUFSZ)) {
    goto free_payload;
  }

  return payload;
free_payload:
  free(payload);
fail:
  return NULL;
}

static void payload_free(struct payload_data *payload) {
  if (payload) {
    buf_cleanup(&payload->buf);
    free(payload);
  }
}

static int append_host_value(buf_t *buf, struct payload_host *h) {
  char hoststr[128];
  char portstr[8];
  int ret;

  ret = getnameinfo(h->sa, h->salen, hoststr, sizeof(hoststr), portstr,
      sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);
  if (ret != 0) {
    return -1;
  }

  if (h->name == NULL || *h->name == '\0') {
    if (h->sa->sa_family != AF_INET) {
      buf_achar(buf, '[');
    }

    buf_adata(buf, hoststr, strlen(hoststr));

    if (h->sa->sa_family != AF_INET) {
      buf_achar(buf, ']');
    }
  } else {
    buf_adata(buf, h->name, strlen(h->name));
  }

  if (strcmp(portstr, "80") != 0 && strcmp(portstr, "443") != 0) {
    buf_achar(buf, ':');
    buf_adata(buf, portstr, strlen(portstr));
  }

  return 0;
}

static struct payload_data *payload_build(const char *fmt, ...) {
  struct payload_data *payload;
  va_list ap;
  int ch;
  int ret;

  if (fmt == NULL) {
    return NULL;
  }

  payload = payload_alloc();
  if (payload == NULL) {
    goto fail;
  }

  va_start(ap, fmt);
  while ((ch = *fmt++) != '\0') {
    if (ch != '$') { /* fastpath: verbatim copy */
      buf_achar(&payload->buf, ch);
      continue;
    }

    switch((ch = *fmt++)) { /* $<X> variable substitution */
    case '$': /* Escaped $ sign */
      buf_achar(&payload->buf, '$');
      break;
    case 'H': {/* HTTP Host header value */
      struct payload_host *h = va_arg(ap, struct payload_host *);
      ret = append_host_value(&payload->buf, h);
      if (ret < 0) {
        goto fail_payload_free;
      }
      break;
    }
    default:
      buf_achar(&payload->buf, '$');
      buf_achar(&payload->buf, ch);
      break;
    }
  }

  va_end(ap);
  /* NB: payload->buf is not \0-terminated */
  return payload;

fail_payload_free:
  payload_free(payload);
fail:
  return NULL;
}

int bgrab_init(struct bgrab_ctx *ctx, struct bgrab_opts *opts,
    struct tcpsrc_ctx tcpsrc) {
  char *recvbuf;
  int ret;
  struct dsts_ctx dsts;
  struct ycl_msg msgbuf;
  struct tcpproto_ctx proto;

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

  ret = tcpproto_init(&proto);
  if (ret != 0) {
    ctx->err = BGRABE_NOMEM;
    goto fail_ycl_msg_cleanup;
  }

  buf_init(&ctx->certbuf, INITIAL_CERTBUF_SIZE);
  ctx->tcpsrc  = tcpsrc;
  ctx->dsts    = dsts;
  ctx->msgbuf  = msgbuf;
  ctx->recvbuf = recvbuf;
  ctx->opts    = *opts;
  ctx->proto   = proto;
  return 0;
fail_ycl_msg_cleanup:
  ycl_msg_cleanup(&msgbuf);
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
    tcpproto_cleanup(&ctx->proto);
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

static int hashcerts(void *dst, size_t dstlen, void *src, size_t srclen) {
  struct sha1_ctx ctx;
  int ret = 0;

  ret = sha1_init(&ctx);
  ret |= sha1_update(&ctx, src, srclen);
  ret |= sha1_final(&ctx, dst, dstlen);
  return ret == 0 ? 0 : -1;
}

static void write_banner(struct bgrab_ctx *ctx, struct reaplan_conn *conn,
    const char *banner, size_t bannerlen) {
  struct ycl_msg_banner bannermsg = {{0}};
  struct bgrab_dst *dst;
  int ret;
  size_t sz;
  unsigned short portnr;
  int mflags;
  unsigned char sha1hash[SHA1_DSTLEN];

  dst = reaplan_conn_get_udata(conn);

  /* lookup the port number from the destination */
  if (dst->addr.sa.sa_family == AF_INET6) {
    portnr = ntohs(dst->addr.sin6.sin6_port);
  } else {
    portnr = ntohs(dst->addr.sin.sin_port);
  }

  /* Setup the certificate chain info part of the message, if any */
  buf_clear(&ctx->certbuf);
  reaplan_conn_append_cert_chain(conn, &ctx->certbuf);
  if (ctx->certbuf.len > 0) {
    bannermsg.certs.data = ctx->certbuf.data;
    bannermsg.certs.len = ctx->certbuf.len;

    /* hash the chain, for later deduplication */
    ret = hashcerts(sha1hash, sizeof(sha1hash),
        ctx->certbuf.data, ctx->certbuf.len);
    if (ret == 0) {
      bannermsg.chash.data = (const char*)sha1hash;
      bannermsg.chash.len = sizeof(sha1hash);
    }
  }

  bannermsg.name.data = dst->name;
  bannermsg.name.len = dst->name ? strlen(dst->name) : 0;
  bannermsg.addr.data = (const char *)&dst->addr.sa;
  bannermsg.addr.len = dst->addrlen;
  bannermsg.banner.data = banner;
  bannermsg.banner.len = bannerlen;
  mflags = ctx->opts.ssl_ctx ? TCPPROTO_MATCHF_TLS : 0;
  bannermsg.mpid = tcpproto_match(&ctx->proto, banner, bannerlen, mflags);
  bannermsg.fpid = tcpproto_type_from_port(portnr);
  ret = ycl_msg_create_banner(&ctx->msgbuf, &bannermsg);
  if (ret != YCL_OK) {
    handle_errorf(ctx,
        "write_banner: failed to serialize %zu bytes banner", bannerlen);
    goto ycl_msg_cleanup;
  }

  sz = fwrite(ycl_msg_bytes(&ctx->msgbuf), ycl_msg_nbytes(&ctx->msgbuf), 1,
      ctx->opts.outfile);
  if (sz != 1) {
    handle_errorf(ctx,
        "write_banner: failed to write %zu bytes banner", bannerlen);
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
