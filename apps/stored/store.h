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
#ifndef CLID_STORE_H__
#define CLID_STORE_H__

#include <lib/util/buf.h>
#include <lib/ycl/ycl.h>
#include <lib/util/eds.h>

#define STORE_CLI(cli__) \
    (struct store_cli*)((cli__)->udata)

/* limits */
#define STORE_IDSZ         20
#define STORE_PREFIXSZ      2 /* must be smaller than STORE_IDSZ */
#define STORE_MAXPATH     128
#define STORE_MAXDIRPATH  \
    STORE_PREFIXSZ + 1 + STORE_IDSZ + 1 + STORE_MAXPATH + 1

#define STORE_ID(ecli) \
    ((ecli)->store_path + STORE_PREFIXSZ + 1)

struct store_cli {
  int flags;
  struct ycl_ctx ycl;
  struct ycl_msg msgbuf;
  char store_path[STORE_IDSZ + STORE_PREFIXSZ + 2]; /* "%s/%s" */
  int open_fd;
  int open_errno;
  buf_t buf; /* scratch buffer, used for "list" action */
};

int store_init(struct eds_service *svc);
void store_fini(struct eds_service *svc);
void store_on_readable(struct eds_client *cli, int fd);
void store_on_finalize(struct eds_client *cli);

#endif
