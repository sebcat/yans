#ifndef YANS_STORECLI_H__
#define YANS_STORECLI_H__

#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var"
#endif

#define STORECLI_DFLPATH LOCALSTATEDIR "/yans/stored/stored.sock"

#define STORECLI_OK         0
#define STORECLI_ERR       -1

#define STORECLI_MAXENTERED 64

struct storecli_ctx {
  struct ycl_ctx *ycl;
  struct ycl_msg *msgbuf;
  const char *err;
  char entered_id[STORECLI_MAXENTERED];
};

static inline void storecli_init(struct storecli_ctx *ctx,
    struct ycl_ctx *ycl, struct ycl_msg *msgbuf) {
  ctx->ycl = ycl;
  ctx->msgbuf = msgbuf;
  ctx->err = NULL;
  ctx->entered_id[0] = '\0';
}

static inline const char *storecli_strerror(struct storecli_ctx *ctx) {
  return ctx->err != NULL ? ctx->err : "no/unknown storecli error";
}

static inline const char *storecli_entered_id(struct storecli_ctx *ctx) {
  return ctx->entered_id;
}

int storecli_enter(struct storecli_ctx *ctx, const char *id,
    const char *name, long indexed);
int storecli_open(struct storecli_ctx *ctx, const char *path, int flags,
    int *outfd);
int storecli_index(struct storecli_ctx *ctx, size_t before, size_t nelems,
    int *outfd);
int storecli_rename(struct storecli_ctx *ctx, const char *from,
    const char *to);


#endif
