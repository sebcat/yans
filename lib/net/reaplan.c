#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef __FreeBSD__
#include <sys/event.h>
#endif
#include <sys/time.h>
#include <errno.h>

#include <lib/net/reaplan.h>

#if defined(__FreeBSD__)

int reaplan_init(struct reaplan_ctx *ctx, struct reaplan_opts *opts) {
  ctx->seq = 0;
  ctx->nconnections = 0;
  ctx->opts = *opts;
  ctx->fd = kqueue();
  if (ctx->fd < 0) {
    return REAPLAN_ERR;
  }

  return REAPLAN_OK;
}

void reaplan_cleanup(struct reaplan_ctx *ctx) {
  if (ctx) {
    if (ctx->fd >= 0) {
      close(ctx->fd);
      ctx->fd = -1;
    }
  }
}

static inline void reset_closefds(struct reaplan_ctx *ctx) {
  ctx->nclosefds = 0;
}

static void closefds(struct reaplan_ctx *ctx) {
  int i;
  struct reaplan_closefd *f;

  for (i = 0; i < ctx->nclosefds; i++) {
    f = ctx->closefds + i;
    /* man 2 kevent: "Calling close() on a file descriptor will remove
     * any kevents that reference the descriptor." */
    close(f->fd);
    if (ctx->opts.funcs.on_done) {
      ctx->opts.funcs.on_done(ctx, f->fd, f->err);
    }
    ctx->nconnections--;
  }
}

/* queues an fd for later closing by closefds */
static int closefd(struct reaplan_ctx *ctx, int fd, int err) {
  int i;

  if (ctx->nclosefds == CONNS_PER_SEQ) {
    return REAPLAN_ERR;
  }

  for (i = 0; i < ctx->nclosefds; i++) {
    if (ctx->closefds[i].fd == fd) {
      return REAPLAN_OK;
    }
  }

  ctx->closefds[i].fd = fd;
  ctx->closefds[i].err = err;
  ctx->nclosefds++;
  return REAPLAN_OK;
}

static int setup_connections(struct reaplan_ctx * restrict ctx,
    struct kevent * restrict evs, size_t * restrict nevs) {
  size_t i = 0;
  size_t end = *nevs;
  struct reaplan_conn conn;
  int ret = REAPLANC_ERR;

  while (i < (end - 2)) {
    conn.fd = -1;
    conn.events = 0;
    ret = ctx->opts.funcs.on_connect(ctx, &conn);
    if (ret != REAPLANC_OK || conn.fd < 0) {
      break;
    } else if (!(conn.events &
        (REAPLAN_READABLE | REAPLAN_WRITABLE_ONESHOT))) {
      close(conn.fd);
      if (ctx->opts.funcs.on_done) {
        ctx->opts.funcs.on_done(ctx, conn.fd, 0);
      }
      continue;
    }

    ctx->nconnections++;
    if (conn.events & REAPLAN_READABLE) {
      EV_SET(&evs[i++], conn.fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    }

    if (conn.events & REAPLAN_WRITABLE_ONESHOT) {
      EV_SET(&evs[i++], conn.fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0,
         NULL);
    }
  }

  *nevs = i;
  return ret;
}

static void handle_readable(struct reaplan_ctx *ctx, int fd) {
  ssize_t ret;
  /* NB: the callback should exist if we listen for readable events */
  ret = ctx->opts.funcs.on_readable(ctx, fd);
  if (ret <= 0) {
    closefd(ctx, fd, ret < 0 ? errno : 0);
  }
}

static void handle_writable(struct reaplan_ctx *ctx, int fd) {
  ssize_t ret;
  /* NB: the callback should exist if we listen for writable events */
  ret = ctx->opts.funcs.on_writable(ctx, fd);
  if (ret <= 0) {
    closefd(ctx, fd, ret < 0 ? errno : 0);
  }
}

int reaplan_run(struct reaplan_ctx *ctx) {
  struct kevent evs[CONNS_PER_SEQ * 2];
  size_t nevs;
  int connect_done = 0;
  int ret;
  struct timespec tv = {.tv_sec = 0, .tv_nsec = 50000000}; /* FIXME */
  struct timespec *tp = &tv;
  int i;

  while (!connect_done || ctx->nconnections > 0) {
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

    ret = kevent(ctx->fd, evs, nevs, evs, CONNS_PER_SEQ, tp);
    if (ret < 0) {
      return REAPLAN_ERR;
    }

    nevs = ret;
    reset_closefds(ctx);
    for (i = 0; i < nevs; i++) {
      if (evs[i].flags & EV_ERROR) {
        closefd(ctx, evs[i].ident, evs[i].data);
        continue;
      }

      if (evs[i].filter == EVFILT_READ) {
        handle_readable(ctx, evs[i].ident);
      } else if (evs[i].filter == EVFILT_WRITE) {
        handle_writable(ctx, evs[i].ident);
      }
    }

    closefds(ctx);
  }

  return REAPLAN_OK;
}

#elif defined(__linux__)

/* TODO: Implement */
int reaplan_init(struct reaplan_ctx *ctx, struct reaplan_opts *opts) {
  return REAPLAN_ERR;
}

void reaplan_cleanup(struct reaplan_ctx *ctx) {
}

int reaplan_run(struct reaplan_ctx *ctx) {
  return REAPLAN_ERR;
}

#else
#error "NYI"
#endif
