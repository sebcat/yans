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
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <lib/net/tcpsrc.h>

#define TCPSRC_PATH "/dev/tcpsrc"

int tcpsrc_fdinit(struct tcpsrc_ctx *ctx, int fd) {
  ctx->fd = fd;
  return 0;
}

int tcpsrc_init(struct tcpsrc_ctx *ctx) {
  int fd;

  fd = open(TCPSRC_PATH, O_RDWR);
  if (fd < 0) {
    return -1;
  }

  return tcpsrc_fdinit(ctx, fd);
}

void tcpsrc_cleanup(struct tcpsrc_ctx *ctx) {
  close(ctx->fd);
  ctx->fd = -1;
}

int tcpsrc_connect(struct tcpsrc_ctx *ctx, struct sockaddr *sa) {
  struct tcpsrc_conn c = {{{0}}};

  switch (sa->sa_family) {
  case AF_INET:
    memcpy(&c.u.sin, sa, sizeof(struct sockaddr_in));
    break;
  case AF_INET6:
    memcpy(&c.u.sin6, sa, sizeof(struct sockaddr_in6));
    break;
  default:
    errno = EINVAL;
    return -1;
  }

  return ioctl(ctx->fd, TCPSRC_CONNECT, &c);
}

