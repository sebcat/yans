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
#ifndef YANS_BGRAB_H__
#define YANS_BGRAB_H__

#include <openssl/ssl.h>

#include <lib/match/tcpproto.h>
#include <lib/net/dsts.h>
#include <lib/net/tcpsrc.h>
#include <lib/ycl/ycl.h>

enum bgrab_err {
  BGRABE_NOERR = 0,
  BGRABE_INVAL_MAX_CLIENTS,
  BGRABE_INVAL_CONNECTS_PER_TICK,
  BGRABE_TOOHIGH_CONNECTS_PER_TICK,
  BGRABE_INVAL_OUTFILE,
  BGRABE_NOMEM,
  BGRABE_INVAL_DST,
  BGRABE_RP_INIT,
  BGRABE_RP_RUN,
};

/* banner grabber options */
struct bgrab_opts {
  int max_clients;
  int timeout;
  int connects_per_tick;
  int mdelay_per_tick;
  void (*on_error)(const char *);
  FILE *outfile;
  SSL_CTX *ssl_ctx;
};

/* banner grabber context */
struct bgrab_ctx {
  /* internal */
  struct bgrab_opts opts;
  struct tcpsrc_ctx tcpsrc;
  struct ycl_msg msgbuf;
  struct dsts_ctx dsts;
  char *recvbuf;
  enum bgrab_err err;
  buf_t certbuf;
  struct tcpproto_ctx proto;
};

#define bgrab_get_recvbuf(b_) (b_)->recvbuf

int bgrab_init(struct bgrab_ctx *ctx, struct bgrab_opts *opts,
    struct tcpsrc_ctx tcpsrc);

void bgrab_cleanup(struct bgrab_ctx *ctx);

int bgrab_add_dsts(struct bgrab_ctx *ctx, const char *addrs,
    const char *ports, void *udata);

int bgrab_run(struct bgrab_ctx *ctx);

const char *bgrab_strerror(struct bgrab_ctx *ctx);

#endif
