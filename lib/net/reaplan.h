#ifndef REAPLAN_H__
#define REAPLAN_H__

#include <lib/util/idset.h>


/* reaplan status codes */
#define REAPLAN_ERR -1
#define REAPLAN_OK   0

/* signals from on_connect callback */
#define REAPLANC_ERR  -1
#define REAPLANC_OK    0
#define REAPLANC_DONE  1
#define REAPLANC_WAIT  2

/* reaplan connection flags */
#define REAPLAN_READABLE         (1 << 0)
#define REAPLAN_WRITABLE_ONESHOT (1 << 1)

struct reaplan_ctx;

struct reaplan_conn {
  /* internal */
  time_t expires;
  void *udata;
  int fd;
  unsigned int flags;      /* set flags */
  unsigned int rflags;     /* requested flags */
};

struct reaplan_opts {
  /* reaplan callbacks */
  int (*on_connect)(struct reaplan_ctx *, struct reaplan_conn *);
  ssize_t (*on_readable)(struct reaplan_ctx *, struct reaplan_conn *);
  ssize_t (*on_writable)(struct reaplan_ctx *, struct reaplan_conn *);
  void (*on_done)(struct reaplan_ctx *, struct reaplan_conn *);

  void *udata;                /* user-data per reaplan instance */
  int timeout;                /* read/write/connect timeout, in seconds */
  unsigned int max_clients;   /* maximum # of concurrent connections */
  unsigned int connects_per_tick; /* # of initiated connections per tick */
};

struct reaplan_ctx {
  /* internal */
  struct kevent *evs;
  struct idset_ctx *ids;      /* idset to keep track of used conn slots */
  struct reaplan_conn *conns; /* array of per-connection state structs */
  struct reaplan_conn **closeconns; /* conns queued for close(2)  */
  struct reaplan_opts opts;   /* caller supplied options */
  time_t last_time;           /* last fetched time, in seconds */
  int fd;                     /* kqueue fd */
  int active_conns;           /* # of currently active connections */
  int ncloseconns;              /* # of currently used closeconns slots */
  int timer_active;           /* 1 if the timeout timer is active */
  int connect_done;           /* 1 if on_connect returned REAPLANC_DONE */
};

int reaplan_init(struct reaplan_ctx *ctx,
    const struct reaplan_opts *opts);
void reaplan_cleanup(struct reaplan_ctx *ctx);
int reaplan_run(struct reaplan_ctx *ctx);

static inline void *reaplan_get_udata(const struct reaplan_ctx *ctx) {
  return ctx->opts.udata;
}

static inline int reaplan_conn_get_fd(const struct reaplan_conn *conn) {
  return conn->fd;
}

static inline void reaplan_conn_register(struct reaplan_conn *conn, int fd,
    unsigned int flags) {
  conn->fd = fd;
  conn->rflags = flags;
}

#endif
