#ifndef REAPLAN_H__
#define REAPLAN_H__

/* reaplan status codes */
#define REAPLAN_ERR -1
#define REAPLAN_OK   0

/* signals from on_connect callback */
#define REAPLANC_ERR  -1
#define REAPLANC_OK    0
#define REAPLANC_DONE  1
#define REAPLANC_WAIT  2

/* reaplan event flags */
#define REAPLAN_READABLE         (1 << 0)
#define REAPLAN_WRITABLE_ONESHOT (1 << 1)

/* maximum number of connections to perform per iteration in reaplan_run */
#define CONNS_PER_SEQ 64

struct reaplan_ctx;

struct reaplan_conn {
  int fd;
  int events; /* REAPLAN_READABLE, REAPLAN_WRITABLE_ONESHOT, ... */
};

struct reaplan_funcs {
  int (*const on_connect)(struct reaplan_ctx *, struct reaplan_conn *);
  ssize_t (*const on_readable)(struct reaplan_ctx *, int);
  ssize_t (*const on_writable)(struct reaplan_ctx *, int);
  void (*const on_done)(struct reaplan_ctx *, int, int);
};

struct reaplan_opts {
  struct reaplan_funcs funcs;
  void *data;
};

struct reaplan_closefd {
  /* internal */
  int fd;
  int err;
};

struct reaplan_ctx {
  /* internal */
  struct reaplan_opts opts;
  int fd; /* kqueue, epoll fd */
  int nconnections;
  unsigned int seq;
  struct reaplan_closefd closefds[CONNS_PER_SEQ];
  int nclosefds;
};

#define reaplan_get_seq(ctx__) (ctx__)->seq
#define reaplan_get_data(ctx__) (ctx__)->opts.data

int reaplan_init(struct reaplan_ctx *ctx, struct reaplan_opts *opts);
void reaplan_cleanup(struct reaplan_ctx *ctx);
int reaplan_run(struct reaplan_ctx *ctx);

#endif
