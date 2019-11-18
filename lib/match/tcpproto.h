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
#ifndef YANS_TCPPROTO_H__
#define YANS_TCPPROTO_H__

#include <lib/net/tcpproto_types.h>
#include <lib/match/reset.h>

#define TCPPROTO_MATCHF_TLS (1 << 0) /* assume TLS encapsulation */

struct tcpproto_ctx {
  /* internal */
  reset_t *reset;
  const char *err;
};

int tcpproto_init(struct tcpproto_ctx *ctx);
void tcpproto_cleanup(struct tcpproto_ctx *ctx);
enum tcpproto_type tcpproto_match(struct tcpproto_ctx *ctx,
    const char *data, size_t len, int flags);



#endif
