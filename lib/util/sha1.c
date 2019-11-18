/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
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
