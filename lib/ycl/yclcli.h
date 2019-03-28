#ifndef YANS_YCLCLI_H__
#define YANS_YCLCLI_H__

#include <lib/ycl/ycl.h>

struct yclcli_ctx {
  /* internal */
  struct ycl_ctx ycl;
  struct ycl_msg *msgbuf; /* can be shared among many clis */
  const char *err;
};

static inline const char *yclcli_strerror(struct yclcli_ctx *ctx) {
  return ctx->err != NULL ? ctx->err : "no/unknown error";
}

static inline int yclcli_seterr(struct yclcli_ctx *ctx,
    const char *err) {
  ctx->err = err;
  return YCL_ERR;
}

void yclcli_init(struct yclcli_ctx *ctx, struct ycl_msg *msgbuf);
int yclcli_connect(struct yclcli_ctx *ctx, const char *path);
int yclcli_close(struct yclcli_ctx *ctx);

#endif
