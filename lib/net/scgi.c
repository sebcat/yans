#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <lib/util/netstring.h>
#include <lib/util/io.h>
#include <lib/net/scgi.h>

#define DFL_HDRBUF (1 << 15)

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define SCGIF_HAS_HEADER (1 << 0)

#define SETERR(ctx__, ...) \
  snprintf((ctx__)->errbuf, sizeof((ctx__)->errbuf), __VA_ARGS__)

int scgi_init(struct scgi_ctx *ctx, int fd, size_t maxhdrsz) {
  assert(ctx != NULL);

  if (buf_init(&ctx->buf, MIN(DFL_HDRBUF, maxhdrsz)) == NULL) {
    return SCGI_EMEM;
  }

  ctx->flags = 0;
  ctx->fd = fd;
  ctx->maxhdrsz = maxhdrsz;
  ctx->nextoff = 0;
  ctx->hdr = NULL;
  ctx->hdrlen = 0;
  ctx->errbuf[0] = '\0';
  return SCGI_OK;
}

void scgi_cleanup(struct scgi_ctx *ctx) {
  if (ctx) {
    buf_cleanup(&ctx->buf);
    ctx->fd = -1;
    ctx->maxhdrsz = 0;
  }
}

int scgi_adata(struct scgi_ctx *ctx, const char *data, size_t len) {
  int ret;

  assert(ctx != NULL);
  ret = buf_adata(&ctx->buf, data, len);
  if (ret < 0) {
    return SCGI_EMEM;
  }

  ret = netstring_tryparse(ctx->buf.data, ctx->buf.len, &ctx->nextoff);
  if (ret != NETSTRING_ERRINCOMPLETE && ret != NETSTRING_OK) {
    return SCGI_EPARSE;
  }

  return SCGI_OK;
}

int scgi_read_header(struct scgi_ctx *ctx) {
  int ret;
  size_t nread;
  io_t io;

  assert(ctx != NULL);

  /* check if we already have the header buffered up */
  if (ctx->flags & SCGIF_HAS_HEADER) {
    return SCGI_OK;
  }

  IO_INIT(&io, ctx->fd);
  while (ctx->buf.len == 0 ||
      (ret = netstring_tryparse(ctx->buf.data, ctx->buf.len,
       &ctx->nextoff)) == NETSTRING_ERRINCOMPLETE) {
    ret = io_readbuf(&io, &ctx->buf, &nread);
    if (ret == IO_AGAIN) {
      return SCGI_AGAIN;
    } else if (ret != IO_OK) {
      return SCGI_EIO;
    } else if (nread == 0) {
      return SCGI_EPREMATURE_CONN_TERM;
    } else if (ctx->buf.len >= ctx->maxhdrsz) {
      SETERR(ctx, "message too large");
      return SCGI_EMSG_TOO_LARGE;
    }
  }

  if (ret != NETSTRING_OK) {
    goto parse_err;
  }

  ctx->flags |= SCGIF_HAS_HEADER;
  return SCGI_OK;

parse_err:
  return SCGI_EPARSE;
}

int scgi_parse_header(struct scgi_ctx *ctx) {
  int ret;

  assert(ctx != NULL);

  /* return success if header is already parsed */
  if (ctx->hdr) {
    return SCGI_OK;
  }

  ret = netstring_parse(&ctx->hdr, &ctx->hdrlen, ctx->buf.data, ctx->buf.len);
  if (ret != NETSTRING_OK) {
    return SCGI_EPARSE;
  }

  return SCGI_OK;
}

int scgi_get_next_header(struct scgi_ctx *ctx, struct scgi_header *hdr) {
  const char *start;
  size_t len;

  if (ctx->hdr == NULL) {
    return SCGI_ENO_HEADER;
  }

  /* if the last header is outside of the header buffer (typically NULL)
   * we start at the beginning. Otherwise, we start after the last one */
  if (hdr->value < ctx->hdr || hdr->value >= (ctx->hdr + ctx->hdrlen)) {
    start = ctx->hdr;
  } else {
    start = hdr->value + hdr->valuelen + 1;
    if (start < ctx->hdr || start >= (ctx->hdr + ctx->hdrlen)) {
      return SCGI_OK; /* We're done */
    }
  }

  len = strlen(start);
  if (len == 0) {
    return SCGI_OK; /* treat keylen == 0 as end-of-headers */
  }

  hdr->key = start;
  hdr->keylen = len;

  start += len + 1;
  if (start >= (ctx->hdr + ctx->hdrlen)) {
    return SCGI_EPARSE;
  }

  hdr->value = start;
  hdr->valuelen = strlen(hdr->value);
  return SCGI_AGAIN;
}

const char *scgi_get_rest(struct scgi_ctx *ctx, size_t *len) {
  size_t nlen = 0;
  const char *res = NULL;

  assert(ctx != NULL);

  if (ctx->nextoff > 0) {
    nlen = ctx->buf.len - ctx->nextoff;
    if (nlen > 0) {
      res = ctx->buf.data + ctx->nextoff;
    }
  }

  if (len != NULL) {
    *len = nlen;
  }

  return res;
}

const char *scgi_strerror(int code) {
  switch(code) {
  case SCGI_OK:
    return "success";
  case SCGI_AGAIN:
    return "again";
  case SCGI_EMEM:
    return "memory allocation error";
  case SCGI_EIO:
    return "i/o error";
  case SCGI_EPREMATURE_CONN_TERM:
    return "premature connection termination";
  case SCGI_EMSG_TOO_LARGE:
    return "message too large";
  case SCGI_EPARSE:
    return "parse error";
  case SCGI_ENO_HEADER:
    return "no/missing/unparsed header";
  default:
    return "unknown error";
  }
}

