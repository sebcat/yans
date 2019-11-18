/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
/* dnstres - threaded DNS resolver
 *
 * creates a threadpool for DNS (A, AAAA) lookups using getaddrinfo,
 * getnameinfo.
 *
 * The reason this exists is because
 *   1) getaddrinfo, getnameinfo are blocking
 *   2) getaddrinfo, getnameinfo are complex
 *
 * In the normal case when we resolve hosts, we want to behave the same way
 * the system resolver does, so we might as well use it. Especially if we have
 * a local caching resolver on the system, or configured search paths, or a
 * number of other cases.
 *
 * Since this is threaded, care should be taken when using it in processes that
 * needs to handle signals (e.g., daemons).
 */

#ifndef YANS_DNSTRES_H__
#define YANS_DNSTRES_H__

#include <stddef.h>

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
  void (*on_unresolved)(void *data, const char *host);
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
