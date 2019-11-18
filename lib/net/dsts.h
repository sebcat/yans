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
#ifndef YANS_DSTS_H__
#define YANS_DSTS_H__

#include <lib/util/buf.h>
#include <lib/net/ip.h>
#include <lib/net/ports.h>

struct dst_ctx {
  /* internal */
  struct ip_blocks addrs;
  struct port_ranges ports;
  void *data;
};

struct dsts_ctx {
  /* internal */
  int flags;
  buf_t buf;                  /* array of struct dst */
  size_t dsts_next;           /* index into 'dsts' for the next dst */
  struct dst_ctx currdst;     /* current dst from dsts */
  ip_addr_t curraddr;         /* current IP address from currdst */
};

int dsts_init(struct dsts_ctx *dsts);
void dsts_cleanup(struct dsts_ctx *dsts);

int dsts_add(struct dsts_ctx *dsts, const char *addrs, const char *ports,
    void *dstdata);
int dsts_next(struct dsts_ctx *dsts, struct sockaddr *dst,
    socklen_t *dstlen, void **data);


#endif
