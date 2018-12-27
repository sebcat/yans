/* vim: set tabstop=2 shiftwidth=2 expandtab: */

#ifndef __FreeBSD__
#error "NYI"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
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

  if (ctx->ncloseconns == CONNS_PER_SEQ) {
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

  for (i = 0; i < opts->max_clients; i++) {
    conns[i].fd = -1;
  }
  ctx->opts = *opts;
  ctx->fd = fd;
  ctx->ids = ids;
  ctx->conns = conns;
  return REAPLAN_OK;

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
  if (ret <= 0) {
    enqueue_close_conn(ctx, conn);
  }
}

static void handle_writable(struct reaplan_ctx *ctx,
    struct reaplan_conn *conn) {
  ssize_t ret;
  /* NB: the callback should exist if we listen for writable events */
  ret = ctx->opts.on_writable(ctx, conn);
  if (ret <= 0) {
    enqueue_close_conn(ctx, conn);
  }
}

static int setup_connections(struct reaplan_ctx * restrict ctx,
    struct kevent * restrict evs, size_t * restrict nevs,
    time_t expires) {
  size_t i = 0;
  size_t end = *nevs;
  int result = REAPLANC_ERR;
  int conn_id;
  struct reaplan_conn *curr;

  while (i < (end - 2)) {
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

  *nevs = i;
  return result;
}

static int reaplan_update_timer(struct reaplan_ctx *ctx,
    struct kevent *ev) {
  struct reaplan_conn *curr;
  struct timespec tv = {0};
  size_t i;

  if (ctx->timer_active || ctx->opts.timeout <= 0) {
    return 0;
  }

  /* if at least a second has passed since last time, iterate over the
   * active connections and close expired ones, if any */
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

  if (ctx->connect_done || ctx->active_conns == ctx->opts.max_clients) {
    /* We have no more connections to establish or we are currently at max
     * capacity. Set the timer to check for connection expirations
     * periodically. */
    EV_SET(ev, TIMEOUT_TIMER, EVFILT_TIMER,
      EV_ADD | EV_ONESHOT, NOTE_SECONDS, SECONDS_TOCHECK, NULL);
  } else {
    /* There are more connections to be made, wait for the designated
     * tick time */
    EV_SET(ev, TIMEOUT_TIMER, EVFILT_TIMER,
      EV_ADD | EV_ONESHOT, NOTE_MSECONDS, MSECONDS_CWAIT, NULL);
  }

  ctx->timer_active = 1;
  return 1;
}

static void reaplan_handle_events(struct reaplan_ctx *ctx,
    struct kevent *evs, size_t nevs) {
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
      /* Check if there's any data to read */
      if (evs[i].data > 0) {
        handle_readable(ctx, evs[i].udata);
      }

      /* Check if EOF is set */
      if (evs[i].flags & EV_EOF) {
        enqueue_close_conn(ctx, evs[i].udata);
      }
    } else if (evs[i].filter == EVFILT_WRITE) {
      if (evs[i].flags & EV_EOF) {
        enqueue_close_conn(ctx, evs[i].udata);
      } else {
        handle_writable(ctx, evs[i].udata);
      }
    } else if (evs[i].filter == EVFILT_TIMER &&
        evs[i].ident == TIMEOUT_TIMER) {
      ctx->timer_active = 0;
    }
  }

  /* close any file descriptors in done/EOF/error state. The distinction
   * between done and EOF is that done is signaled from the callbacks
   * whereas EOF is signaled from the file descriptors. */
  close_conns(ctx);
}

int reaplan_run(struct reaplan_ctx *ctx) {
  /* evs: up to two (EV_READ, EV_WRITE) events per conn, + 1 timer */
  struct kevent evs[CONNS_PER_SEQ*2 + 1];
  size_t nevs = 0;
  size_t new_nevs = 0;
  int ret;
  struct timespec tv = {0};
  time_t expires = 0;

  /* setup initial context state for 'run' */
  ctx->timer_active = 0;
  ctx->connect_done = 0;

  /* setup initial timeout timer event, if any */
  if (reaplan_update_timer(ctx, evs)) {
    nevs++;
  }

  while (!ctx->connect_done || ctx->active_conns > 0) {
    /* check if there is more connections to be made. If there is, set
     * them up. */
    if (!ctx->connect_done) {
      /* calculate connection expiration for the new connections, if any */
      if (ctx->opts.timeout > 0) {
        ret = clock_gettime(CLOCK_MONOTONIC, &tv);
        if (ret == 0) {
          expires = tv.tv_sec + ctx->opts.timeout;
        }
      }

      new_nevs = CONNS_PER_SEQ * 2;
      ret = setup_connections(ctx, evs + nevs, &new_nevs, expires);
      nevs += new_nevs;
      if (ret == REAPLANC_DONE) {
        ctx->connect_done = 1;
      } else if (ret == REAPLANC_ERR) {
        return REAPLAN_ERR;
      }
      /* NB: REAPLANC_WAIT is not explicitly handled */
    }

kevent_again:
    ret = kevent(ctx->fd, evs, nevs, evs, CONNS_PER_SEQ, NULL);
    if (ret < 0) {
      if (errno == EINTR) {
        goto kevent_again;
      } else {
        return REAPLAN_ERR;
      }
    }

    /* handle events returned from kevent(2) */
    reaplan_handle_events(ctx, evs, (size_t)ret);

    /* setup the next round of events, if any */
    nevs = 0;
    /* setup timer for next round, if any */
    if (reaplan_update_timer(ctx, evs)) {
      nevs++;
    }
  }

  return REAPLAN_OK;
}

