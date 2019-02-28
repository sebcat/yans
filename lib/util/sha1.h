#ifndef YANS_SHA1_H__
#define YANS_SHA1_H__

#include <openssl/sha.h>

#define SHA1_DSTLEN 20

struct sha1_ctx {
  /* internal */
  SHA_CTX c;
};

int sha1_init(struct sha1_ctx *ctx);
int sha1_update(struct sha1_ctx *ctx, const void *data, size_t len);
int sha1_final(struct sha1_ctx *ctx, void *dst, size_t len);

#endif
