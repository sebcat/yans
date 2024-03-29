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
#ifndef YANS_YCL_H__
#define YANS_YCL_H__

#define YCL_ERRBUFSIZ 128
#define YCL_IDATASIZ   32

#define YCL_AGAIN   1
#define YCL_OK      0
#define YCL_ERR    -1

#define YCL_NOFD   -1 /* e.g., ycl_init(ycl, YCL_NOFD) */

/* ycl_msg.flags */
#define YCLMSGF_HASOPTBUF    (1 << 0)

#include <stdint.h>
#include <stdio.h> /* due to ycl_readmsg */

#include <lib/util/buf.h>

struct ycl_ctx {
  /* -- internal -- */
  int fd;
  int flags;
  size_t max_msgsz;
  char errbuf[YCL_ERRBUFSIZ];
};

struct ycl_msg {
  /* -- internal -- */
  buf_t buf;
  buf_t mbuf; /* secondary buffer used as a scratch-pad in create/parse */
  buf_t optbuf; /* optional buffer used by create for array elements */
  size_t sendoff; /* sendmsg message offset */
  size_t nextoff; /* message offset to next received chunk, if any */
  int flags;      /* flags used internally */
};

/* accessor macros for individual fields */
#define ycl_fd(ycl) \
    (ycl)->fd
#define ycl_msg_bytes(msg) \
    (msg)->buf.data
#define ycl_msg_nbytes(msg) \
    (msg)->buf.len


void ycl_init(struct ycl_ctx *ycl, int fd);
int ycl_connect(struct ycl_ctx *ycl, const char *dst);
int ycl_close(struct ycl_ctx *ycl);
const char *ycl_strerror(struct ycl_ctx *ycl);
int ycl_setnonblock(struct ycl_ctx *ycl, int status);

/* ycl_readmsg is *only* for use with YCL_NOFD ycl_ctx'es on files */
int ycl_readmsg(struct ycl_ctx *ycl, struct ycl_msg *msg, FILE *fp);

int ycl_sendmsg(struct ycl_ctx *ycl, struct ycl_msg *msg);
int ycl_recvmsg(struct ycl_ctx *ycl, struct ycl_msg *msg);
int ycl_recvfd(struct ycl_ctx *ycl, int *fd);
int ycl_sendfd(struct ycl_ctx *ycl, int fd, int err);

int ycl_msg_init(struct ycl_msg *msg);
int ycl_msg_set(struct ycl_msg *msg, const void *data, size_t len);
int ycl_msg_use_optbuf(struct ycl_msg *msg);
void ycl_msg_reset(struct ycl_msg *msg);
void ycl_msg_cleanup(struct ycl_msg *msg);

#endif  /* YANS_YCL_H__ */
