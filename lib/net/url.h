#ifndef URL_H_
#define URL_H_

#define EURL_OK         0
#define EURL_MEM       -1
#define EURL_PARSE     -2
#define EURL_SCHEME    -3
#define EURL_HOST      -4

#include <lib/util/buf.h>

#define URLFL_REMOVE_EMPTY_QUERY    (1 << 0)
#define URLFL_REMOVE_EMPTY_FRAGMENT (1 << 1)
struct url_opts {
  int flags;
};

typedef struct url_ctx_t url_ctx_t;

#define URLPART_HAS_USERINFO (1 << 0)
#define URLPART_HAS_PORT     (1 << 2)
#define URLPART_HAS_QUERY    (1 << 3)
#define URLPART_HAS_FRAGMENT (1 << 4)
struct url_parts {
  int flags;
  size_t scheme, schemelen;
  size_t auth, authlen;
  size_t userinfo, userinfolen;
  size_t host, hostlen;
  size_t port, portlen;
  size_t path, pathlen;
  size_t query, querylen;
  size_t fragment, fragmentlen;
};

url_ctx_t *url_ctx_new(struct url_opts *opts);
void url_ctx_free(url_ctx_t *ctx);
const struct url_opts *url_ctx_opts(url_ctx_t *ctx);

/* URL scheme validation
 *
 * return EURL_OK if the scheme is supported by this library, EURL_SCHEME
 * if it is not
 */
int url_supported_scheme(const char *s);
/* RFC3986 5.2.1 parse function
 *
 * Splits a URL string into its corresponding parts. No normalization
 * or validation is performed in this step, or in url_parse_auth . */
int url_parse(url_ctx_t *ctx, const char *s, struct url_parts *out);

/**
 * remove_dot_segments from RFC3986 5.2.4
 *   modifies s in-place to remove dot segments. Returns the new length
 *   of the path. The new path is not null terminated.
 */
size_t url_remove_dot_segments(char *s, size_t len);

/* normalizes an HTTP/HTTPS URL
 *
 * 'out' must be initialized before this call, using 'buf_init'
 *
 * returns EURL_OK on success or a negative error code on error */
int url_normalize(url_ctx_t *ctx, const char *s, buf_t *out);

/* resolves an HTTP/HTTPS reference URL to a base URL according to
 * RFC3986 5.2.1. The base URL is not normalized in this function.
 *
 * 'out' must be initialized before this call, using 'buf_init'
 *
 * return EURL_OK on success or a negative error code on error */
int url_resolve(url_ctx_t *ctx, const char *base, const char *ref, buf_t *out);

const char *url_errstr(int code);

#endif
