/* vim: set tabstop=2 shiftwidth=2 expandtab: */
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#if defined(__FreeBSD__)
#include <sys/event.h>
#elif defined(__linux__)
#include <sys/epoll.h>
#endif
#include <sys/time.h>
#include <errno.h>

#include <lib/net/reaplan.h>

static inline void reset_closefds(struct reaplan_ctx *ctx) {
  ctx->nclosefds = 0;
}

static void closefds(struct reaplan_ctx *ctx) {
  int i;
  struct reaplan_closefd *f;

  for (i = 0; i < ctx->nclosefds; i++) {
    f = ctx->closefds + i;
    /* man 2 kevent: "Calling close() on a file descriptor will remove
     * any kevents that reference the descriptor."
     * I have no idea what happens with epoll */
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

int reaplan_init(struct reaplan_ctx *ctx, struct reaplan_opts *opts) {
  ctx->seq = 0;
  ctx->nconnections = 0;
  ctx->opts = *opts;
#if defined(__FreeBSD__)
  ctx->fd = kqueue();
#elif defined(__linux__)
  ctx->fd = epoll_create1(EPOLL_CLOEXEC);
#endif
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

#if defined(__FreeBSD__)

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

#define EVDATA_SET(ev_, fd_, events_) \
    (ev_)->data.u64 = ((((uint64_t)fd_) << 32) | \
    (((uint64_t)events_) & 0xffffffff))

#define EVDATA_FD(ev_) \
    ((int)(((ev_)->data.u64 >> 32) & 0xffffffff))

#define EVDATA_EVENTS(ev_) \
    ((int)(((ev_)->data.u64 & 0xffffffff)))

static uint32_t to_epoll_events(int reaplan_event) {
  uint32_t events = 0;
  if (reaplan_event & REAPLAN_READABLE) {
    events |= EPOLLIN;
  }

  if (reaplan_event & REAPLAN_WRITABLE_ONESHOT) {
    /* NB: we do not use EPOLLONESHOT - instead we remove EPOLLOUT in 
     * handle_writable with EPOLL_CTL_MOD. This is because epoll handles
     * file descriptors instead of events, and EPOLLONESHOT is set for
     * a file descriptor instead of an event. One of the reasons why
     * kqueue/kevent is superior. Another reason why kqueue/kevent is
     * superior is because there's no need for a call like
     * epoll_ctl to begin with. */
    events |= EPOLLOUT;
  }

  return events;
}

static int setup_connections(struct reaplan_ctx *ctx) {
  size_t i = 0;
  struct reaplan_conn conn;
  int ret = REAPLANC_ERR;
  int ctlret;
  struct epoll_event ev;

  for (i = 0; i < CONNS_PER_SEQ; i++) {
    conn.fd = -1;
    conn.events = 0;
    ret = ctx->opts.funcs.on_connect(ctx, &conn);
    if (ret != REAPLANC_OK || conn.fd < 0) {
      break;
    }

    ev.events = to_epoll_events(conn.events);
    if (ev.events == 0) {
      close(conn.fd);
      if (ctx->opts.funcs.on_done) {
        ctx->opts.funcs.on_done(ctx, conn.fd, 0);
      }

      continue;
    }

    EVDATA_SET(&ev, conn.fd, conn.events);
    ctlret = epoll_ctl(ctx->fd, EPOLL_CTL_ADD, conn.fd, &ev);
    if (ctlret != 0) {
      close(conn.fd);
      if (ctx->opts.funcs.on_done) {
        ctx->opts.funcs.on_done(ctx, conn.fd, errno);
      }
      return REAPLANC_ERR;
    }
    ctx->nconnections++;
  }

  return ret;
}

int reaplan_run(struct reaplan_ctx *ctx) {
  struct epoll_event events[CONNS_PER_SEQ];
  int connect_done = 0;
  int ret;
  int nevs;
  int i;

  /* while we still have future or ongoing connections to manage */
  while (!connect_done || ctx->nconnections > 0) {
    if (!connect_done) {
      /* initiate new connections */
      ret = setup_connections(ctx);
      if (ret == REAPLANC_DONE) {
        connect_done = 1;
      } else if (ret == REAPLANC_ERR) {
        return REAPLAN_ERR;
      }
    }

    /* wait for events on file descriptors */
    ret = epoll_wait(ctx->fd, events, CONNS_PER_SEQ, 50); /* TODO: FIXME */
    if (ret < 0) {
      return REAPLAN_ERR;
    }

    /* handle file descriptor events */
    nevs = ret;
    reset_closefds(ctx);
    for (i = 0; i < nevs; i++) {
      struct epoll_event *ev = events + i;
      int fd = EVDATA_FD(ev);
      int evflags = EVDATA_EVENTS(ev);

      if ((ev->events & (EPOLLERR | EPOLLHUP)) != 0) {
        closefd(ctx, fd, 0); /* TODO: correct errno, if any? */
        continue;
      }

      if (ev->events & EPOLLIN) {
        handle_readable(ctx, fd);
      }

      if (ev->events & EPOLLOUT) {
        if (evflags & REAPLAN_WRITABLE_ONESHOT) {
          evflags &= ~REAPLAN_WRITABLE_ONESHOT;
          EVDATA_SET(ev, fd, evflags);

          ev->events = to_epoll_events(evflags);
          epoll_ctl(ctx->fd, EPOLL_CTL_MOD, fd, ev);
        }
        handle_writable(ctx, fd);
      }
    }

    closefds(ctx);
  }

  return REAPLAN_OK;
}

#else
#error "NYI"
#endif
