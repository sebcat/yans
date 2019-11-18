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
#ifndef KNEGD_KNG_H__
#define KNEGD_KNG_H__

#include <lib/ycl/ycl.h>
#include <lib/util/eds.h>

#define KNG_CLI(cli__) \
    (struct kng_cli*)((cli__)->udata)

#ifndef DATAROOTDIR
#define DATAROOTDIR "/usr/local/share"
#endif

#ifndef DFL_KNEGDIR
#define DFL_KNEGDIR DATAROOTDIR "/kneg"
#endif

#ifndef DFL_QUEUEDIR
#define DFL_QUEUEDIR "queue"
#endif

#ifndef DFL_NQUEUESLOTS
#define DFL_NQUEUESLOTS 7
#endif

#define DFL_TIMEOUT 43200 /* default process timeout, in seconds */

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var"
#endif

#define DFL_STORESOCK LOCALSTATEDIR "/yans/stored/stored.sock"


struct kng_cli {
  int flags;
  struct ycl_ctx ycl;
  struct ycl_msg msgbuf;
  buf_t respbuf;
};

void kng_set_knegdir(const char *dir);
void kng_set_queuedir(char *dir);
void kng_set_nqueueslots(int nslots);
void kng_set_timeout(long timeout);
void kng_set_storesock(const char *path);

void kng_on_readable(struct eds_client *cli, int fd);
void kng_on_svc_reaped_child(struct eds_service *svc, pid_t pid,
    int status);
void kng_on_finalize(struct eds_client *cli);
int kng_mod_init(struct eds_service *svc);
void kng_mod_fini(struct eds_service *svc);
void kng_on_tick(struct eds_service *svc);

#endif
