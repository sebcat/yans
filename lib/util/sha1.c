#include <lib/util/sha1.h>

int sha1_init(struct sha1_ctx *ctx) {
  int ret;

  ret = SHA1_Init(&ctx->c);
  return ret == 1 ? 0 : -1;
}

int sha1_update(struct sha1_ctx *ctx, const void *data, size_t len) {
  int ret;

  ret = SHA1_Update(&ctx->c, data, len);
  return ret == 1 ? 0 : -1;
}

int sha1_final(struct sha1_ctx *ctx, void *dst, size_t len) {
  int ret;

  if (len < SHA1_DSTLEN) {
    return -1;
  }

  ret = SHA1_Final(dst, &ctx->c);
  return ret == 1 ? 0 : -1;
}
