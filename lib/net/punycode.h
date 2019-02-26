#ifndef PUNYCODE_H_
#define PUNYCODE_H_

#include <stddef.h>

#include <lib/util/buf.h>

struct punycode_ctx {
  buf_t outbuf;
  buf_t lblbuf;
};

int punycode_init(struct punycode_ctx *ctx);
void punycode_cleanup(struct punycode_ctx *ctx);

char *punycode_encode(struct punycode_ctx *ctx, const char *in, size_t len);

#endif
