#ifndef YANS_BGRAB_H__
#define YANS_BGRAB_H__

#include <lib/net/dsts.h>
#include <lib/net/tcpsrc.h>
#include <lib/ycl/ycl.h>

/* banner grabber options */
struct bgrab_opts {
  int max_clients;
  int timeout;
  int connects_per_tick;
  int mdelay_per_tick;
  void (*on_error)(const char *);
  FILE *outfile;
};

/* banner grabber context */
struct bgrab_ctx {
  /* internal */
  struct bgrab_opts opts;
  struct tcpsrc_ctx tcpsrc;
  struct ycl_msg msgbuf;
  struct dsts_ctx dsts;
  char *recvbuf; /* TODO: Maybe inline? */
};

#define bgrab_get_recvbuf(b_) (b_)->recvbuf

int bgrab_init(struct bgrab_ctx *ctx, struct bgrab_opts *opts,
    struct tcpsrc_ctx tcpsrc);

void bgrab_cleanup(struct bgrab_ctx *ctx);

int bgrab_add_dsts(struct bgrab_ctx *ctx, const char *addrs,
    const char *ports, void *udata);

int bgrab_run(struct bgrab_ctx *ctx);

#endif
