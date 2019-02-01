#ifndef YANS_RESOLVERCLI_H__
#define YANS_RESOLVERCLI_H__

#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var"
#endif

#define RESOLVERCLI_DFLPATH LOCALSTATEDIR "/yans/clid/resolver.sock"

#define RESOLVERCLI_OK         0
#define RESOLVERCLI_ERR       -1

struct resolvercli_ctx {
  /* internal */
  struct ycl_ctx *ycl;
  struct ycl_msg *msgbuf;
  const char *err;
};

static inline void resolvercli_init(struct resolvercli_ctx *ctx,
    struct ycl_ctx *ycl, struct ycl_msg *msgbuf) {
  ctx->ycl = ycl;
  ctx->msgbuf = msgbuf;
  ctx->err = NULL;
}

static inline const char *resolvercli_strerror(struct resolvercli_ctx *ctx) {
  return ctx->err != NULL ? ctx->err : "no/unknown error";
}

int resolvercli_resolve(struct resolvercli_ctx *ctx, int dstfd,
    const char *spec, size_t speclen, int compress);


#endif
