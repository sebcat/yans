/* dnstres - threaded DNS resolver
 *
 * The reason this exists is because
 *   1) getaddrinfo, getnameinfo are blocking
 *   2) getaddrinfo, getnameinfo are complex
 *
 * we want to behave the same way the system resolver does, so we might
 * as well use it. Especially if we have a local caching resolver on the
 * system, or configured search paths, or a number of other cases.
 */

#ifndef YANS_DNSTRES_H__
#define YANS_DNSTRES_H__

/* string representing a set of delimiters */
#define DTR_DELIMS "\r\n\t ,"


typedef struct dtr_pool dtr_pool_t;

struct dtr_pool_opts {
  unsigned short nthreads;
};

/* resolve request. Callbacks will be called from threads, so lock your data */
struct dtr_request {
  /* hosts: \0-terminated string of hosts to resolve separated by char(s)
   * in DTR_DELIMS */
  const char *hosts;
  void (*on_resolved)(void *data, const char *host, const char *addr);
  void (*on_done)(void *data);
  void *data;
};

/* create a new resolver pool */
dtr_pool_t *dtr_pool_new(struct dtr_pool_opts *opts);
void dtr_pool_free(dtr_pool_t *p);
void dtr_pool_add_hosts(struct dtr_pool *p, struct dtr_request *req);

#endif
