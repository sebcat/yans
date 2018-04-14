#ifndef NET_SCGI_H__
#define NET_SCGI_H__

/* This is a minimal SCGI parser implementation, which mainly takes care of
 * header parsing and buffering for blocking/non-blocking I/O. No validation
 * of header fields are performed, and no response message handling is done
 * here. */

#include <stddef.h>

#include <lib/util/buf.h>

#define SCGI_DEFAULT_MAXHDRSZ (1 << 20)

#define SCGI_AGAIN                   (1)
#define SCGI_OK                      (0)
#define SCGI_EMEM                   (-1)
#define SCGI_EIO                    (-2)
#define SCGI_EPREMATURE_CONN_TERM   (-3)
#define SCGI_EMSG_TOO_LARGE         (-4)
#define SCGI_EPARSE                 (-5)
#define SCGI_ENO_HEADER             (-6)

struct scgi_ctx {
  int flags;
  int fd;
  buf_t buf;
  size_t maxhdrsz;
  size_t nextoff;
  char *hdr;
  size_t hdrlen;
  char errbuf[128];
};

struct scgi_header {
  const char *key;
  size_t keylen;
  const char *value;
  size_t valuelen;
};

int scgi_init(struct scgi_ctx *ctx, int fd, size_t maxhdrsz);
void scgi_cleanup(struct scgi_ctx *ctx);

int scgi_adata(struct scgi_ctx *ctx, const char *data, size_t len);
int scgi_read_header(struct scgi_ctx *ctx);
int scgi_parse_header(struct scgi_ctx *ctx);

/* return SCGI_AGAIN if we have retrieved a header, SCGI_DONE when there are
 * no headers left, and SCGI_ERR on error */
int scgi_get_next_header(struct scgi_ctx *ctx, struct scgi_header *hdr);

const char *scgi_get_rest(struct scgi_ctx *ctx, size_t *len);
const char *scgi_strerror(int code);

#endif
