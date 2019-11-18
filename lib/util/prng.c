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
/* Mersenne twister for Mersenne prime 2^(19937-1) */

#include "prng.h"

#define        N          624 /* degree of recurrence */
#define        M          397 /* middle word */
#define    UMASK   0x80000000
#define    LMASK   0x7fffffff

void prng_init(struct prng_ctx *ctx, uint32_t seed) {
  ctx->t[0] = seed;
  ctx->mag[0] = 0;
  ctx->mag[1] = 0x9908b0df;
  for(ctx->i = 1; ctx->i < N; ctx->i++) {
    ctx->t[ctx->i] = 1812433253 *
        (ctx->t[ctx->i - 1] ^ (ctx->t[ctx->i - 1] >> 30)) + ctx->i;
  }
}

uint32_t prng_uint32(struct prng_ctx *ctx) {
  uint32_t i;
  uint32_t j;

  if (ctx->i >= N) {
    for (j = 0; j < N - M; j++) {
      i = (ctx->t[j] & UMASK) | (ctx->t[j + 1] & LMASK);
      ctx->t[j] = ctx->t[j + M] ^ (i >> 1) ^ ctx->mag[i & 1];
    }

    for (; j < N-1; j++) {
      i = (ctx->t[j] & UMASK) | (ctx->t[j + 1] & LMASK);
      ctx->t[j] = ctx->t[j + (M-N)] ^ (i >> 1) ^ ctx->mag[i & 1];
    }

    i = (ctx->t[N - 1] & UMASK) | (ctx->t[0] & LMASK);
    ctx->t[N - 1] = ctx->t[M - 1] ^ (i >> 1) ^ ctx->mag[i & 1];
    ctx->i = 0;
  }

  i = ctx->t[ctx->i++];
  i ^= (i >> 11);
  i ^= (i << 7) & 0x9d2c5680;
  i ^= (i << 15) & 0xefc60000;
  i ^= (i >> 18);
  return i;
}

void prng_hex(struct prng_ctx *ctx, char *data, size_t len) {
  static const char *syms = "0123456789abcdef";
  uint32_t rnd = 0;
  size_t i;

  for (i = 0; i < len; i++) {
    if ((i & 0x07) == 0) {
      rnd = prng_uint32(ctx);
    }
    data[i] = syms[(rnd >> (4 * (7 - (i & 0x07)))) & 0x0f];
  }
}
