/* dnstres - threaded DNS resolver
 *
 * creates a threadpool for DNS lookups.
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

typedef struct dnstres_pool dnstres_pool_t;

struct dnstres_pool_opts {
  unsigned short nthreads; /* number of threads in the threadpool */
  size_t stacksize; /* thread stack size -- be careful with this one */
};

/* resolve request. Callbacks will be called from threads, so lock your data */
struct dnstres_request {
  /* hosts: \0-terminated string of hosts to resolve separated by char(s)
   * in DTR_DELIMS */
  const char *hosts;
  void (*on_resolved)(void *data, const char *host, const char *addr);
  void (*on_done)(void *data);
  void *data;
};

/* create a new resolver pool */
dnstres_pool_t *dnstres_pool_new(struct dnstres_pool_opts *opts);

/* free an existing pool - currently ongoing requests needs to be completed
 * but queued requests will be removed */
void dnstres_pool_free(dnstres_pool_t *p);

/* add a list of domains to the resolve queue */
void dnstres_pool_add_hosts(struct dnstres_pool *p, struct dnstres_request *req);

#endif
