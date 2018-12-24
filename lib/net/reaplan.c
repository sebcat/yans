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

#include <lib/net/reaplan.h>

static inline void reset_closefds(struct reaplan_ctx *ctx) {
  ctx->nclosefds = 0;
}

static void closefds(struct reaplan_ctx *ctx) {
  int i;
  struct reaplan_conn *conn;

  for (i = 0; i < ctx->nclosefds; i++) {
    conn = ctx->closefds[i];
    /* man 2 kevent: "Calling close() on a file descriptor will remove
     * any kevents that reference the descriptor."
     * I have no idea what happens with epoll */
    close(conn->fd);
    if (ctx->opts.on_done) {
      ctx->opts.on_done(ctx, conn);
    }
    ctx->active_conns--;
  }
}

/* queues an fd for later closing by closefds */
static int closefd(struct reaplan_ctx *ctx, struct reaplan_conn *conn) {
  int i;

  if (ctx->nclosefds == CONNS_PER_SEQ) {
    return REAPLAN_ERR;
  }

  for (i = 0; i < ctx->nclosefds; i++) {
    if (ctx->closefds[i] == conn) {
      return REAPLAN_OK;
    }
  }

  ctx->closefds[i] = conn;
  ctx->nclosefds++;
  return REAPLAN_OK;
}

int reaplan_init(struct reaplan_ctx *ctx,
    const struct reaplan_opts *opts) {
  int fd;
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
  if (ctx) {
    idset_free(ctx->ids);
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
    closefd(ctx, conn);
  }
}

static void handle_writable(struct reaplan_ctx *ctx,
    struct reaplan_conn *conn) {
  ssize_t ret;
  /* NB: the callback should exist if we listen for writable events */
  ret = ctx->opts.on_writable(ctx, conn);
  if (ret <= 0) {
    closefd(ctx, conn);
  }
}

static int setup_connections(struct reaplan_ctx * restrict ctx,
    struct kevent * restrict evs, size_t * restrict nevs) {
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

    result = ctx->opts.on_connect(ctx, curr);
    if (result != REAPLANC_OK || curr->fd < 0) {
      idset_clear(ctx->ids, conn_id);
      break;
    } else if (!(curr->rflags &
        (REAPLAN_READABLE | REAPLAN_WRITABLE_ONESHOT))) {
      close(curr->fd);
      if (ctx->opts.on_done) {
        ctx->opts.on_done(ctx, curr);
      }
      idset_clear(ctx->ids, conn_id);
      continue;
    }

    ctx->active_conns++;
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

int reaplan_run(struct reaplan_ctx *ctx) {
  struct kevent evs[CONNS_PER_SEQ * 2];
  size_t nevs;
  int connect_done = 0;
  int ret;
  struct timespec tv = {.tv_sec = 0, .tv_nsec = 50000000}; /* FIXME */
  struct timespec *tp = &tv;
  int i;

  while (!connect_done || ctx->active_conns > 0) {
    if (!connect_done) {
      nevs = sizeof(evs) / sizeof(struct kevent);
      ret = setup_connections(ctx, evs, &nevs);
      if (ret == REAPLANC_DONE) {
        connect_done = 1;
      } else if (ret == REAPLANC_ERR) {
        return REAPLAN_ERR;
      }
    } else {
      nevs = 0;
      tp = NULL;
    }

kevent_again:
    ret = kevent(ctx->fd, evs, nevs, evs, CONNS_PER_SEQ, tp);
    if (ret < 0) {
      if (errno == EINTR) {
        goto kevent_again;
      } else {
        return REAPLAN_ERR;
      }
    }

    nevs = ret;
    reset_closefds(ctx);
    for (i = 0; i < nevs; i++) {
      if (evs[i].flags & EV_ERROR) {
        /* NB: evs[i].data contains errno */
        closefd(ctx, evs[i].udata);
        continue;
      }

      if (evs[i].filter == EVFILT_READ) {
        /* Do we have data to read? */
        if (evs[i].data > 0) {
          handle_readable(ctx, evs[i].udata);
        }

        /* is EOF set? */
        if (evs[i].flags & EV_EOF) {
          closefd(ctx, evs[i].udata);
        }
      } else if (evs[i].filter == EVFILT_WRITE) {
        if (evs[i].flags & EV_EOF) {
          closefd(ctx, evs[i].udata);
        } else {
          handle_writable(ctx, evs[i].udata);
        }
      }
    }

    closefds(ctx);
  }

  return REAPLAN_OK;
}

