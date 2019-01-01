/* vim: set tabstop=2 shiftwidth=2 expandtab: */

#ifndef __FreeBSD__
#error "NYI"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <sys/time.h>
#include <errno.h>

#include <assert.h>

#include <lib/net/reaplan.h>

#define TIMEOUT_TIMER 666 /* ident for timeout timer */

/* interval in seconds to check for expired connections */
#define SECONDS_TOCHECK 2

/* number of milliseconds to wait between ticks */
#define MSECONDS_CWAIT 25

static inline int reaplan_conn_id(struct reaplan_ctx * restrict ctx,
    struct reaplan_conn * restrict conn) {
  if (conn < ctx->conns || conn >= ctx->conns + ctx->opts.max_clients) {
    return -1;
  }

  return (int)(conn - ctx->conns);
}

static void reaplan_conn_close(struct reaplan_ctx *ctx,
    struct reaplan_conn *conn) {
  int id;

  /* man 2 kevent: "Calling close() on a file descriptor will remove
   * any kevents that reference the descriptor." */
  close(conn->fd);
  conn->fd = -1;
  if (ctx->opts.on_done) {
    ctx->opts.on_done(ctx, conn);
  }
  if (conn->ssl) {
    SSL_free(conn->ssl);
    conn->ssl = NULL;
  }
  ctx->active_conns--;
  id = reaplan_conn_id(ctx, conn);
  assert(id >= 0);
  idset_clear(ctx->ids, id);
}

static inline void reset_close_queue(struct reaplan_ctx *ctx) {
  ctx->ncloseconns = 0;
}

static void close_conns(struct reaplan_ctx *ctx) {
  int i;

  for (i = 0; i < ctx->ncloseconns; i++) {
    reaplan_conn_close(ctx, ctx->closeconns[i]);
  }
}

/* queues an connection for later closing by close_conns */
static int enqueue_close_conn(struct reaplan_ctx *ctx,
    struct reaplan_conn *conn) {
  int i;

  if (ctx->ncloseconns == ctx->opts.connects_per_tick) {
    return REAPLAN_ERR;
  }

  for (i = 0; i < ctx->ncloseconns; i++) {
    if (ctx->closeconns[i] == conn) {
      return REAPLAN_OK;
    }
  }

  ctx->closeconns[i] = conn;
  ctx->ncloseconns++;
  return REAPLAN_OK;
}

int reaplan_init(struct reaplan_ctx *ctx,
    const struct reaplan_opts *opts) {
  int fd;
  unsigned int i;
  struct idset_ctx *ids;
  struct reaplan_conn *conns;
  struct reaplan_conn **closeconns;
  struct kevent *evs;

  /* Sanity-check options */
  if (opts->on_connect == NULL ||
      opts->max_clients == 0 ||
      opts->connects_per_tick == 0 ||
      opts->connects_per_tick > opts->max_clients) {
    return REAPLAN_ERR;
  }

  memset(ctx, 0, sizeof(*ctx));
  fd = kqueue();
  if (fd < 0) {
    goto fail;
  }

  ids = idset_new(opts->max_clients);
  if (ids == NULL) {
    goto fail_close_fd;
  }

  conns = calloc(opts->max_clients, sizeof(struct reaplan_conn));
  if (conns == NULL) {
    goto fail_idset_free;
  }

  closeconns = calloc(opts->connects_per_tick,
      sizeof(struct reaplan_conn *));
  if (closeconns == NULL) {
    goto fail_conns_free;
  }

  /* evs: up to two (EV_READ, EV_WRITE) events per conn, + 1 timer */
  evs = calloc(opts->connects_per_tick*2 + 1, sizeof(struct kevent));
  if (evs == NULL) {
    goto fail_closeconns_free;
  }

  for (i = 0; i < opts->max_clients; i++) {
    conns[i].fd = -1;
  }
  ctx->evs = evs;
  ctx->opts = *opts;
  ctx->fd = fd;
  ctx->ids = ids;
  ctx->conns = conns;
  ctx->closeconns = closeconns;
  return REAPLAN_OK;

fail_closeconns_free:
  free(closeconns);
fail_conns_free:
  free(conns);
fail_idset_free:
  idset_free(ids);
fail_close_fd:
  close(fd);
fail:
  return REAPLAN_ERR;
}

void reaplan_cleanup(struct reaplan_ctx *ctx) {
  unsigned int i;

  if (ctx) {
    idset_free(ctx->ids);
    for (i = 0; i < ctx->opts.max_clients; i++) {
      if (ctx->conns[i].fd >= 0) {
        reaplan_conn_close(ctx, &ctx->conns[i]);
      }
    }
    free(ctx->conns);
    free(ctx->closeconns);
    free(ctx->evs);

    if (ctx->fd >= 0) {
      close(ctx->fd);
      ctx->fd = -1;
    }
  }
}

static void handle_readable(struct reaplan_ctx *ctx,
    struct reaplan_conn *conn) {
  ssize_t ret;
  /* NB: the callback should exist if we listen for readable events */
  ret = ctx->opts.on_readable(ctx, conn);
  if (ret == 0 || (ret < 0 && errno != EAGAIN)) {
    enqueue_close_conn(ctx, conn);
  }
}

static void handle_writable(struct reaplan_ctx *ctx,
    struct reaplan_conn *conn) {
  ssize_t ret;
  /* NB: the callback should exist if we listen for writable events */
  ret = ctx->opts.on_writable(ctx, conn);
  if (ret == 0 || (ret < 0 && errno != EAGAIN)) {
    enqueue_close_conn(ctx, conn);
  }
}

int reaplan_register_conn(struct reaplan_ctx *ctx,
    struct reaplan_conn *conn, int fd, unsigned int flags,
    const char *name) {
  int ret;

  if (ctx->opts.ssl_ctx) {
    conn->ssl = SSL_new(ctx->opts.ssl_ctx);
    if (conn->ssl == NULL) {
      return -1;
    }

    ret = SSL_set_fd(conn->ssl, fd);
    if (ret != 1) {
      SSL_free(conn->ssl);
      conn->ssl = NULL;
      return -1;
    }

    if (name != NULL) {
      SSL_set_tlsext_host_name(conn->ssl, name);
    }
  }

  conn->fd = fd;
  conn->rflags = flags;
  return 0;
}


static int setup_connections(struct reaplan_ctx * restrict ctx,
    struct kevent * restrict evs, size_t * restrict nevs,
    time_t expires) {
  size_t i = 0;
  size_t end = *nevs;
  struct reaplan_conn *curr;
  int result = REAPLANC_ERR;
  int conn_id;

  while (i <= (end - 2)) {
    conn_id = idset_use_next(ctx->ids);
    if (conn_id < 0) {
      /* no available IDs */
      result = REAPLANC_WAIT;
      break;
    }

    curr = ctx->conns + conn_id;
    memset(curr, 0, sizeof(*curr));
    curr->fd = -1;

    result = ctx->opts.on_connect(ctx, curr);
    if (result != REAPLANC_OK || curr->fd < 0) {
      idset_clear(ctx->ids, conn_id);
      break;
    }

    ctx->active_conns++;
    if (!(curr->rflags & (REAPLAN_READABLE | REAPLAN_WRITABLE_ONESHOT))) {
      reaplan_conn_close(ctx, curr);
      continue;
    }

    curr->expires = expires;

    if (curr->ssl) {
      curr->flags |= REAPLAN_TLS_HANDSHAKE;
      EV_SET(&evs[i++], curr->fd, EVFILT_READ, EV_ADD, 0, 0, curr);
      EV_SET(&evs[i++], curr->fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0,
          curr);
    } else {
      if (curr->rflags & REAPLAN_READABLE) {
        curr->flags |= REAPLAN_READABLE;
        EV_SET(&evs[i++], curr->fd, EVFILT_READ, EV_ADD, 0, 0, curr);
      }

      if (curr->rflags & REAPLAN_WRITABLE_ONESHOT) {
        curr->flags |= REAPLAN_WRITABLE_ONESHOT;
        EV_SET(&evs[i++], curr->fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0,
           curr);
      }
    }
  }

  *nevs = i;
  return result;
}

static int reaplan_update_timer(struct reaplan_ctx *ctx,
    struct kevent *ev) {
  struct reaplan_conn *curr;
  struct timespec tv = {0};
  size_t i;

  /* if the timer is active there is no need to go further. Return 0 to
   * indicate that we have not set up another timer. */
  if (ctx->timer_active) {
    return 0;
  }

  /* If we have connection expiration configured and if at least a second
   * has passed since last time, iterate over the active connections and
   * close expired ones, if any */
  if (ctx->opts.timeout > 0) {
    clock_gettime(CLOCK_MONOTONIC, &tv);
    if (tv.tv_sec > ctx->last_time) {
      for (i = 0; i < ctx->opts.max_clients && ctx->active_conns > 0; i++) {
        curr = ctx->conns + i;
        if (curr->fd >= 0 && curr->expires <= tv.tv_sec) {
          reaplan_conn_close(ctx, curr);
        }
      }

      ctx->last_time = tv.tv_sec;
    }
  }

  if (ctx->connect_done || ctx->active_conns == ctx->opts.max_clients) {
    /* We have no more connections to establish or we are currently at max
     * capacity. Set the timer to check for connection expirations
     * periodically. */
    EV_SET(ev, TIMEOUT_TIMER, EVFILT_TIMER,
      EV_ADD | EV_ONESHOT, NOTE_SECONDS, SECONDS_TOCHECK, NULL);
  } else if (ctx->opts.mdelay_per_tick <= 0) {
    /* We have more connections to make and no tick time set up - do not
     * register a timeout event */
    return 0;
  } else {
    /* There are more connections to be made, wait for the designated
     * tick time */
    EV_SET(ev, TIMEOUT_TIMER, EVFILT_TIMER, EV_ADD | EV_ONESHOT,
        NOTE_MSECONDS, ctx->opts.mdelay_per_tick, NULL);
  }

  ctx->timer_active = 1;
  return 1;
}

static void handle_tls_handshake(struct reaplan_ctx *ctx,
    struct reaplan_conn *conn) {
  int ret;
  int err;
  struct kevent evs[2];
  int i;

  assert(ctx->opts.ssl_ctx != NULL);
  assert(conn->ssl != NULL);

  ret = SSL_connect(conn->ssl);
  if (ret < 0) {
    err = SSL_get_error(conn->ssl, ret);
    if (err == SSL_ERROR_WANT_WRITE) {
      EV_SET(&evs[0], conn->fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0,
          conn);
      kevent(ctx->fd, evs, 1, NULL, 0, NULL);
    } else if (err != SSL_ERROR_WANT_READ) {
      /* connection/handshake failure */
      enqueue_close_conn(ctx, conn);
    }
  } else if (ret == 0) {
    /* graceful shutdown */
    enqueue_close_conn(ctx, conn);
  } else {
    /* handshake done, set the flags requested by the user. READ is already
     * set */
    conn->flags = 0;
    i = 0;
    if (conn->rflags & REAPLAN_READABLE) {
      conn->flags |= REAPLAN_READABLE;
    } else {
      EV_SET(&evs[i++], conn->fd, EVFILT_READ, EV_DELETE, 0, 0, conn);
    }

    if (conn->rflags & REAPLAN_WRITABLE_ONESHOT) {
      conn->flags |= REAPLAN_WRITABLE_ONESHOT;
      EV_SET(&evs[i++], conn->fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0,
           conn);
    }

    /* We must have at least a readable or writable event registered prior
     * to the handshake */
    assert(i > 0);
    kevent(ctx->fd, evs, i, NULL, 0, NULL);
  }
}

static void reaplan_handle_events(struct reaplan_ctx *ctx,
    struct kevent *evs, size_t nevs) {
  struct reaplan_conn *conn;
  size_t i;

  reset_close_queue(ctx);
  for (i = 0; i < nevs; i++) {
    /* check error for read/write events */
    if ((evs[i].flags & EV_ERROR) != 0 &&
        (evs[i].filter == EVFILT_READ ||
          evs[i].filter == EVFILT_WRITE)) {
      /* NB: evs[i].data contains errno */
      enqueue_close_conn(ctx, evs[i].udata);
      continue;
    }

    if (evs[i].filter == EVFILT_READ) {
      conn = evs[i].udata;

      /* Check if EOF is set */
      if (evs[i].flags & EV_EOF) {
        enqueue_close_conn(ctx, conn);
      }

      /* Check if there's any data to read */
      if (conn->flags & REAPLAN_TLS_HANDSHAKE) {
        handle_tls_handshake(ctx, conn);
      } else if (evs[i].data > 0) {
        handle_readable(ctx, conn);
      }
    } else if (evs[i].filter == EVFILT_WRITE) {
      conn = evs[i].udata;

      if (evs[i].flags & EV_EOF) {
        enqueue_close_conn(ctx, conn);
      } else if (conn->flags & REAPLAN_TLS_HANDSHAKE) {
        handle_tls_handshake(ctx, conn);
      } else {
        handle_writable(ctx, conn);
      }
    } else if (evs[i].filter == EVFILT_TIMER &&
        evs[i].ident == TIMEOUT_TIMER) {
      ctx->timer_active = 0;
      ctx->throttle_connect = 0; /* clear any connection throttling */
    }
  }

  /* close any file descriptors in done/EOF/error state. The distinction
   * between done and EOF is that done is signaled from the callbacks
   * whereas EOF is signaled from the file descriptors. */
  close_conns(ctx);
}

int reaplan_run(struct reaplan_ctx *ctx) {
  size_t nevs = 0;
  size_t new_nevs = 0;
  int ret;
  struct timespec tv = {0};
  struct timespec nodelay = {0};
  struct timespec *evto;
  time_t expires = 0;

  /* setup initial context state for 'run' */
  ctx->timer_active = 0;
  ctx->connect_done = 0;

  /* setup initial timeout timer event, if any */
  if (reaplan_update_timer(ctx, ctx->evs)) {
    nevs++;
  }

  while (!ctx->connect_done || ctx->active_conns > 0) {
    /* check if there is more connections to be made. If there is, set
     * them up. */
    if (!ctx->connect_done &&
        (ctx->opts.mdelay_per_tick <= 0 || !ctx->throttle_connect)) {
      /* calculate connection expiration for the new connections, if any */
      if (ctx->opts.timeout > 0) {
        ret = clock_gettime(CLOCK_MONOTONIC, &tv);
        if (ret == 0) {
          expires = tv.tv_sec + ctx->opts.timeout;
        }
      }

      new_nevs = ctx->opts.connects_per_tick * 2;
      ret = setup_connections(ctx, ctx->evs + nevs, &new_nevs, expires);
      nevs += new_nevs;
      if (ret == REAPLANC_DONE) {
        ctx->connect_done = 1;
      } else if (ret == REAPLANC_ERR) {
        return REAPLAN_ERR;
      }
      /* NB: REAPLANC_WAIT is not explicitly handled */

      /* We have made our connections for this tick - do not allow any
       * more connections to be made until the timeout timer has ticked.
       * If mdelay_per_tick is <= 0 then throttle_connect has no effect. */
      ctx->throttle_connect = 1;
    }

    if (ctx->opts.mdelay_per_tick <= 0 &&
        !ctx->connect_done &&
        ctx->active_conns < ctx->opts.max_clients) {
      /* if we have no delay per tick, still connections to make and
         slots available for new connections - do not wait in kevent(2) */
      evto = &nodelay;
    } else {
      /* wait indefinitely in kevent(2) for events to trigger */
      evto = NULL;
    }

kevent_again:
    ret = kevent(ctx->fd, ctx->evs, nevs, ctx->evs,
        ctx->opts.connects_per_tick, evto);
    if (ret < 0) {
      if (errno == EINTR) {
        goto kevent_again;
      } else {
        return REAPLAN_ERR;
      }
    }

    /* handle events returned from kevent(2) */
    reaplan_handle_events(ctx, ctx->evs, (size_t)ret);

    /* setup the next round of events, if any */
    nevs = 0;
    /* setup timer for next round, if any */
    if (reaplan_update_timer(ctx, ctx->evs)) {
      nevs++;
    }
  }

  return REAPLAN_OK;
}

int reaplan_conn_read(struct reaplan_conn *conn, void *data, int len) {
  int ret;
  int err;

  if (conn->ssl) {
    ret = SSL_read(conn->ssl, data, len);
    if (ret <= 0) {
      err = SSL_get_error(conn->ssl, ret);
      if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        errno = EAGAIN;
      }
    }
    return ret;
  } else {
    return (int)read(conn->fd, data, (size_t)len);
  }
}

int reaplan_conn_write(struct reaplan_conn *conn, void *data, int len) {
  int ret;
  int err;

  if (conn->ssl) {
    ret = SSL_write(conn->ssl, data, len);
    if (ret <= 0) {
      err = SSL_get_error(conn->ssl, ret);
      if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        errno = EAGAIN;
      }
    }
    return ret;

  } else {
    return (int)send(conn->fd, data, (size_t)len, MSG_NOSIGNAL);
  }
}
