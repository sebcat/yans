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
#ifndef YANS_YCLCLI_H__
#define YANS_YCLCLI_H__

#include <lib/ycl/ycl.h>

struct yclcli_ctx {
  /* internal */
  struct ycl_ctx ycl;
  struct ycl_msg *msgbuf; /* can be shared among many clis */
  const char *err;
};

static inline const char *yclcli_strerror(struct yclcli_ctx *ctx) {
  return ctx->err != NULL ? ctx->err : "no/unknown error";
}

static inline int yclcli_seterr(struct yclcli_ctx *ctx,
    const char *err) {
  ctx->err = err;
  return YCL_ERR;
}

void yclcli_init(struct yclcli_ctx *ctx, struct ycl_msg *msgbuf);
int yclcli_connect(struct yclcli_ctx *ctx, const char *path);
int yclcli_close(struct yclcli_ctx *ctx);

#endif
