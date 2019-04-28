#ifndef YANS_OPENER_H__
#define YANS_OPENER_H__

#include <lib/ycl/yclcli_store.h>
#include <lib/ycl/ycl.h>

#define OPENER_FCREAT (1 << 0) /* create store */

struct opener_opts {
  int flags;
  struct ycl_msg *msgbuf;
  const char *socket;
  const char *store_id;
};

struct opener_ctx {
  /* internal */
  struct yclcli_ctx cli;
  struct ycl_msg msgbuf; /* don't use directly - use opts.msgbuf instead */
  struct opener_opts opts;
  const char *err;
  char *allotted_store_id;
  int flags;
};

static inline const char * opener_store_id(struct opener_ctx *ctx) {
  return ctx->opts.store_id;
}

int opener_init(struct opener_ctx *ctx, struct opener_opts *opts);
void opener_cleanup(struct opener_ctx *ctx);
int opener_open(struct opener_ctx *ctx, const char *path, int flags,
    int *use_zlib, int *outfd);
int opener_fopen(struct opener_ctx *ctx, const char *path,
    const char *mode, FILE **fp);
const char *opener_strerror(struct opener_ctx *ctx);

#endif
