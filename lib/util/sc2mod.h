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

