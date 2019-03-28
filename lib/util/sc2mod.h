#ifndef YANS_SC2MOD_H__
#define YANS_SC2MOD_H__

#define SC2MOD_API __attribute__((visibility("default")))

struct sc2mod_ctx {
  const char *err;
  void *data;
};

static inline void *sc2mod_data(struct sc2mod_ctx *ctx) {
  return ctx->data;
}

static inline void sc2mod_set_data(struct sc2mod_ctx *ctx, void *data) {
  ctx->data = data;
}

static inline int sc2mod_error(struct sc2mod_ctx *ctx, const char *err) {
  ctx->err = err;
  return 1; /* think EXIT_FAILURE */
}

static inline const char *sc2mod_strerror(struct sc2mod_ctx *ctx) {
  return ctx->err ? ctx->err : "no/unknown error";
}

#endif

