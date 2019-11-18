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
#ifndef YANS_RESOLVER_H__
#define YANS_RESOLVER_H__

#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>
#include <lib/util/eds.h>

struct resolver_cli {
  int flags;
  struct ycl_ctx ycl;
  struct ycl_msg msgbuf;
  int resfd;
  FILE *resfile; /* the result will be written here, received from client */
  int closefds[2]; /* socketpair fds to signal complete */
  const char *hosts; /* pointer into parsed msgbuf containing hosts */
};

/* must be called before resolver_init */
void resolver_set_nresolvers(unsigned short nresolvers);
int resolver_init(struct eds_service *svc);
void resolver_on_readable(struct eds_client *cli, int fd);
void resolver_on_done(struct eds_client *cli, int fd);
void resolver_on_finalize(struct eds_client *cli);

#endif
