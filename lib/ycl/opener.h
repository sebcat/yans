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
#ifndef YANS_OPENER_H__
#define YANS_OPENER_H__

#include <lib/ycl/yclcli_store.h>
#include <lib/ycl/ycl.h>

#define OPENER_FCREAT (1 << 0) /* create store */

struct opener_opts {
  int flags;
  struct ycl_msg *msgbuf;
  const char *socket;
  const char *store_id;
};

struct opener_ctx {
  /* internal */
  struct yclcli_ctx cli;
  struct ycl_msg msgbuf; /* don't use directly - use opts.msgbuf instead */
  struct opener_opts opts;
  const char *err;
  char *allotted_store_id;
  int flags;
};

static inline const char * opener_store_id(struct opener_ctx *ctx) {
  return ctx->opts.store_id;
}

int opener_init(struct opener_ctx *ctx, struct opener_opts *opts);
void opener_cleanup(struct opener_ctx *ctx);
int opener_open(struct opener_ctx *ctx, const char *path, int flags,
    int *use_zlib, int *outfd);
int opener_fopen(struct opener_ctx *ctx, const char *path,
    const char *mode, FILE **fp);
const char *opener_strerror(struct opener_ctx *ctx);

#endif
