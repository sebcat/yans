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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <limits.h>

#include <lib/net/dnstres.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t donecond = PTHREAD_COND_INITIALIZER;
size_t ncompleted = 0;

static void my_on_done(void *data) {
  pthread_mutex_lock(&mutex);
  ncompleted++;
  pthread_mutex_unlock(&mutex);
  pthread_cond_signal(&donecond);
}

static void my_on_resolved(void *data, const char *host, const char *addr) {
  flockfile(stdout);
  fprintf(stdout, "%s %s\n", host, addr);
  funlockfile(stdout);
}

static void my_on_unresolved(void *data, const char *host) {
  flockfile(stdout);
  fprintf(stdout, "%s\n", host);
  funlockfile(stdout);
}

int main(int argc, char *argv[]) {
  struct dnstres_pool *p;
  size_t nstarted;
  char buf[256];
  int nthreads;
  int stacksize = 0;
  struct dnstres_request req = {
    .on_resolved   = my_on_resolved,
    .on_unresolved = my_on_unresolved,
    .on_done       = my_on_done,
  };
  struct dnstres_pool_opts opts = {0};

  if (argc < 2) {
    fprintf(stderr, "usage: %s <nthreads> [stacksize] \n", argv[0]);
    return EXIT_FAILURE;
  }

  nthreads = atoi(argv[1]);
  if (nthreads < 1 || nthreads > 100) {
    fprintf(stderr, "invalid number of threads\n");
    return EXIT_FAILURE;
  }

  if (argc >= 3) {
    stacksize = atoi(argv[2]);
    if (stacksize <= PTHREAD_STACK_MIN) {
      stacksize = 0;
    }
  }

  opts.nthreads = (unsigned short)nthreads;
  opts.stacksize = (stacksize > 0) ? (size_t)stacksize : 0;
  p = dnstres_pool_new(&opts);
  if (p == NULL) {
    fprintf(stderr, "dnstres_pool_new failure\n");
    return EXIT_FAILURE;
  }

  fprintf(stderr, "started %d threads, stacksize: %d (0=default)\n",
    nthreads, stacksize);

  /* start all the jobs until EOF (ctrl+D or end of piped data). The requests
   * may complete before we start checking done status, but that's OK. */
  for(nstarted = 0; fgets(buf, sizeof(buf), stdin); nstarted++) {
    req.hosts = buf;
    dnstres_pool_add_hosts(p, &req);
  }

  /* wait for completion */
  pthread_mutex_lock(&mutex);
  while (ncompleted < nstarted) {
    pthread_cond_wait(&donecond, &mutex);
  }
  pthread_mutex_unlock(&mutex);
  dnstres_pool_free(p);
  return 0;
}
