#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <lib/net/dnstres.h>

struct dnstres_hosts {
  /* while next and prev are a part of dnstres_hosts, they are guarded by
   * the mutex in dnstres_pool. */
  struct dnstres_hosts *prev;
  struct dnstres_hosts *next;
  _Atomic(int) refcount; /* inited to 1, when it reaches zero, obj is freed */
  struct dnstres_request req; /* not protected by a lock, user must lock */
  pthread_mutex_t host_mutex; /* protects next_host, hosts */
  char *next_host; /* ptr into hosts to the start of the next entry, or end */
  char hosts[];    /* string of DTR_DELIMS separated hosts to resolve */
};

struct dnstres_pool {
  pthread_mutex_t mutex;
  pthread_cond_t nonempty; /* broadcasted condition on insertion */
  bool done;
  const struct dnstres_pool_opts opts;
  /* first_host and last_host are the start and end of a non-circular
   * double-linked list of dnstres_hosts */
  struct dnstres_hosts *first_host;
  struct dnstres_hosts *last_host;
  pthread_t threads[]; /* thread IDs of spawned threads */
};

static void *resolver(void *data); /* resolver thread */

/* create a new dnstres_hosts entry, which contains a NULL-terminated string
 * of DTR_DELIMS-separated hosts to resolve. The '\0'-terminator is added by
 * dnstres_hosts_new when the string is copied from hoststr. The internal
 * string is modified to chunk out pointers to the string for various threads
 * to work with. */
static struct dnstres_hosts *dnstres_hosts_new(struct dnstres_request *req) {
  int ret;
  struct dnstres_hosts *res;
  size_t len;

  if (req->hosts == NULL) {
    return NULL;
  }

  len = strlen(req->hosts);
  res = calloc(1, sizeof(struct dnstres_hosts) + len + 1);
  if (res == NULL) {
    return NULL;
  }

  atomic_init(&res->refcount, 1);
  ret = pthread_mutex_init(&res->host_mutex, NULL);
  if (ret != 0) {
    free(res);
    return NULL;
  }
  memcpy(res->hosts, req->hosts, len + 1);
  res->next_host = res->hosts;
  res->req = *req;
  res->req.hosts = NULL; /* res->req.hosts is caller memory, NULL it out */
  return res;
}

static void _dnstres_hosts_cleanup(struct dnstres_hosts *h) {
  if (h->req.on_done) {
    h->req.on_done(h->req.data);
  }
  pthread_mutex_destroy(&h->host_mutex);
  free(h);
}

/* give back a reference to dnstres_hosts, free it if there's no more
 * references to it */
static inline void dnstres_hosts_put(struct dnstres_hosts *h) {
  /* the '1' is not a typo -- it's fetch_sub and not sub_fetch */
  if (atomic_fetch_sub(&h->refcount, 1) == 1) {
    _dnstres_hosts_cleanup(h);
  }
}

/* get a reference to a dnstres_hosts */
static inline void dnstres_hosts_get(struct dnstres_hosts *h) {
  atomic_fetch_add(&h->refcount, 1);
}

/* modifies h->hosts. Returns a pointer to the next DTR_DELIMS delimited host
 * name, or NULL if the end is reached. the optional 'outlen' is filled with
 * the length of the name, not including the terminator. Multiple threads
 * can hold pointers into h->hosts, which is why dnstres_hosts are
 * refcounted. */
static char *_dnstres_hosts_next(struct dnstres_hosts *h, size_t *outlen) {
  size_t off;
  char *curr;

  /* skip leading delimiters, if any. If we end up on a '\0' we return NULL
   * to signal that we've reached the end. */
  off = strspn(h->next_host, DTR_DELIMS);
  curr = h->next_host + off;
  if (*curr == '\0') {
    return NULL;
  }

  /* find the first delimiter, or the end of string. Set up the length
   * accordingly */
  off = strcspn(curr, DTR_DELIMS);
  if (outlen != NULL) {
    *outlen = off;
  }

  /* if the current offset is a DTR_DELIMS delimiter, replace it with a \0
   * and advance the offset for next_host */
  if (curr[off] != '\0') {
    curr[off] = '\0';
    off++;
  }
  h->next_host = curr + off;

  return curr;
}

static char *dnstres_hosts_next(struct dnstres_hosts *h, size_t *outlen) {
  char *ret;

  pthread_mutex_lock(&h->host_mutex);
  ret = _dnstres_hosts_next(h, outlen);
  pthread_mutex_unlock(&h->host_mutex);
  return ret;
}

void dnstres_pool_free(struct dnstres_pool *p) {
  struct dnstres_hosts *curr;
  struct dnstres_hosts *next;
  size_t i;

  pthread_mutex_lock(&p->mutex);
  curr = p->first_host;
  p->first_host = p->last_host = NULL;
  p->done = true;
  pthread_mutex_unlock(&p->mutex);
  pthread_cond_broadcast(&p->nonempty);
  while (curr) {
    next = curr->next;
    dnstres_hosts_put(curr);
    curr = next;
  }

  for (i = 0; i < (size_t)p->opts.nthreads; i++) {
    if (p->threads[i] != NULL) {
      pthread_join(p->threads[i], NULL);
    }
  }
}

struct dnstres_pool *dnstres_pool_new(struct dnstres_pool_opts *opts) {
  pthread_attr_t attrs;
  unsigned short curr_thread;
  struct dnstres_pool *p;
  int ret;
  /* opts in dnstres_pool should be const, so we set up a temporary struct
   * on the stack and memcpy it to p */
  struct dnstres_pool tmp = {
    .first_host = NULL,
    .last_host = NULL,
    .opts = *opts,
    .done = false,
  };

  if (opts->nthreads == 0) {
    return NULL;
  }

  p = calloc(1, sizeof(*p) + (sizeof(pthread_t) * (size_t)opts->nthreads));
  if (p == NULL) {
    goto fail;
  }

  memcpy(p, &tmp, sizeof(*p));
  ret = pthread_mutex_init(&p->mutex, NULL);
  if (ret != 0) {
    goto cleanup_p;
  }

  ret = pthread_cond_init(&p->nonempty, NULL);
  if (ret != 0) {
    goto cleanup_mutex;
  }

  ret = pthread_attr_init(&attrs);
  if (ret != 0) {
    goto cleanup_cond;
  }

  if (opts->stacksize > PTHREAD_STACK_MIN) {
    pthread_attr_setstacksize(&attrs, opts->stacksize);
  }

  for (curr_thread = 0; curr_thread < opts->nthreads; curr_thread++) {
    ret = pthread_create(&p->threads[curr_thread], NULL, resolver, p);
    if (ret != 0 && curr_thread == 0) {
      /* we couldn't start the first thread - error out */
      goto cleanup_attr;
    }
  }

  /* NB: Since this is intended to run as a daemon, we don't wait for the
   *   threads to actually start. If we call dnstres_pool_add_hosts and follow
   *   that by calling dnstres_pool_free, it is likely that the done flag will
   *   be set before the threads are started, causing them to terminate
   *   immediately without doing any resolving. If we want to resolve
   *   everything before termination, or have any other semantics, that can
   *   usually be implemented in the user callbacks. */

  pthread_attr_destroy(&attrs);
  return p;

cleanup_attr:
  pthread_attr_destroy(&attrs);
cleanup_cond:
  pthread_cond_destroy(&p->nonempty);
cleanup_mutex:
   pthread_mutex_destroy(&p->mutex);
cleanup_p:
  free(p);
fail:
  return NULL;
}

void dnstres_pool_add_hosts(struct dnstres_pool *p,
    struct dnstres_request *req) {
  struct dnstres_hosts *h;

  /* allocate a new hosts entry */
  h = dnstres_hosts_new(req);
  if (h == NULL) {
    if (req->on_done) {
      req->on_done(req->data);
    }
    return;
  }

  /* insert the entry into the list of entries while holding the pool mutex,
   * unless we're done and shutting down, in which case we tell the caller
   * that we added the hosts, but they will not be resolved. */
  pthread_mutex_lock(&p->mutex);
  if (p->done) {
    pthread_mutex_unlock(&p->mutex);
    dnstres_hosts_put(h);
    return;
  } else if (p->last_host == NULL) {
    assert(p->first_host == NULL);
    p->first_host = p->last_host = h;
  } else {
    p->last_host->next = h;
    h->prev = p->last_host;
    p->last_host = h;
  }
  pthread_mutex_unlock(&p->mutex);
  pthread_cond_broadcast(&p->nonempty);
}

/* NB: needs to be called while holding p->mutex */
static char *dnstres_pool_get_host(struct dnstres_pool *p, size_t *hostlen,
    struct dnstres_hosts **outh) {
  char *cptr;
  struct dnstres_hosts *h;

  if (p->first_host == NULL) {
    /* we have no host entries at all, exit */
    goto empty;
  }

  while ((cptr = dnstres_hosts_next(p->first_host, hostlen)) == NULL) {
    /* we have a host entry, but it's depleted. Remove it. */
    h = p->first_host->next;
    dnstres_hosts_put(p->first_host);
    p->first_host = h;

    if (h == NULL) {
      /* the next entry is empty, we're out of hosts */
      p->last_host = NULL;
      goto empty;
    }
    /* the next entry is potentially non-empty. Clear it's prev pointer and
     * try again. XXX: are we using the prev pointers at all? */
    h->prev = NULL;
  }

  /* at this point, we have a non-empty host value pointed to by cptr. This
   * value points into a dnstres_hosts structure, so we take a reference to it.
   * The reference count must be decremented when we're done with the host
   * value, which is why we pass along the dnstres_hosts entry to the caller */
  dnstres_hosts_get(p->first_host);
  *outh = p->first_host;
  return cptr;

empty:
  return NULL;
}

static void resolve(struct dnstres_hosts *h, const char *host,
    size_t hostlen) {
  struct addrinfo hints = {0};
  struct addrinfo *addrs;
  struct addrinfo *curr;
  char addrbuf[128];
  int ret;

  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM; /* to avoid duplicate entries */
  ret = getaddrinfo(host, NULL, &hints, &addrs);
  if (ret != 0) {
    return;
  }

  for (curr = addrs; curr != NULL; curr = curr->ai_next) {
    if (!(curr->ai_family == AF_INET || curr->ai_family == AF_INET6)) {
      continue;
    }

    ret = getnameinfo(curr->ai_addr, curr->ai_addrlen, addrbuf,
        sizeof(addrbuf), NULL, 0, NI_NUMERICHOST);
    if (ret == 0 && h->req.on_resolved) {
      h->req.on_resolved(h->req.data, host, addrbuf);
    }
  }

  freeaddrinfo(addrs);
}

/* resolver thread */
static void *resolver(void *data) {
  char *host;
  size_t hostlen;
  struct dnstres_pool *p = data;
  struct dnstres_hosts *h;

  while (1) {
    pthread_mutex_lock(&p->mutex);
again:
    if (p->done) {
      pthread_mutex_unlock(&p->mutex);
      return NULL;
    }
    if ((host = dnstres_pool_get_host(p, &hostlen, &h)) == NULL) {
      pthread_cond_wait(&p->nonempty, &p->mutex);
      goto again;
    }
    pthread_mutex_unlock(&p->mutex);
    resolve(h, host, hostlen);
    dnstres_hosts_put(h);
  }
}

