#ifndef YANS_STORECLI_H__
#define YANS_STORECLI_H__

#include <lib/ycl/ycl.h>

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var"
#endif

#define STORECLI_DFLPATH LOCALSTATEDIR "/yans/stored/stored.sock"

#define STORECLI_MAXENTERED 64

struct storecli_ctx {
  /* internal */
  struct ycl_ctx ycl;
  struct ycl_msg *msgbuf; /* can be shared among many clis */
  const char *err;
  char entered_id[STORECLI_MAXENTERED];
};

static inline const char *storecli_strerror(struct storecli_ctx *ctx) {
  return ctx->err != NULL ? ctx->err : "no/unknown error";
}

static inline const char *storecli_entered_id(struct storecli_ctx *ctx) {
  return ctx->entered_id;
}

void storecli_init(struct storecli_ctx *ctx, struct ycl_msg *msgbuf);
int storecli_connect(struct storecli_ctx *ctx, const char *path);
int storecli_close(struct storecli_ctx *ctx);
int storecli_enter(struct storecli_ctx *ctx, const char *id,
    const char *name, long indexed);
int storecli_open(struct storecli_ctx *ctx, const char *path, int flags,
    int *outfd);
int storecli_list(struct storecli_ctx *ctx, const char *id,
    const char *must_match, const char **result, size_t *resultlen);
int storecli_index(struct storecli_ctx *ctx, size_t before, size_t nelems,
    int *outfd);
int storecli_rename(struct storecli_ctx *ctx, const char *from,
    const char *to);


#endif
