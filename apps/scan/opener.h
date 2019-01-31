#ifndef SCAN_OPENER_H__
#define SCAN_OPENER_H__

#include <lib/ycl/storecli.h>
#include <lib/ycl/ycl.h>

struct opener_opts {
  struct ycl_msg *msgbuf;
  const char *socket;
  const char *store_id;
};

struct opener_ctx {
  /* internal */
  struct storecli_ctx cli;
  struct ycl_ctx ycl;
  struct ycl_msg msgbuf; /* don't use directly - use opts.msgbuf instead */
  struct opener_opts opts;
  const char *err;
  int flags;
};

int opener_init(struct opener_ctx *ctx, struct opener_opts *opts);
void opener_cleanup(struct opener_ctx *ctx);
int opener_fopen(struct opener_ctx *ctx, const char *path,
    const char *mode, FILE **fp);
const char *opener_strerr(struct opener_ctx *ctx);

#endif
