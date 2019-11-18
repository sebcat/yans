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

#if defined(__FreeBSD__)
#include <sys/capsicum.h>

int sandbox_enter() {
  if (cap_enter() == -1) {
    return -1;
  }

  return 0;
}

#elif defined(__linux__)
#include <stddef.h>
#include <errno.h>
#include <seccomp.h>

int sandbox_enter() {
  int calls[] ={
    SCMP_SYS(brk),
    SCMP_SYS(epoll_create),
    SCMP_SYS(epoll_create1),
    SCMP_SYS(epoll_ctl),
    SCMP_SYS(epoll_wait),
    SCMP_SYS(exit),
    SCMP_SYS(exit_group),
    SCMP_SYS(fstat),
    SCMP_SYS(futex),
    SCMP_SYS(mmap),
    SCMP_SYS(mprotect),
    SCMP_SYS(munmap),
    SCMP_SYS(poll),
    SCMP_SYS(read),
    SCMP_SYS(sched_getaffinity),
    SCMP_SYS(sched_setaffinity),
    SCMP_SYS(sched_yield),
    SCMP_SYS(select),
    SCMP_SYS(write),
  };
  int i;
    int ret = -1;
  scmp_filter_ctx ctx = NULL;

  if ((ctx = seccomp_init(SCMP_ACT_ERRNO(EACCES))) == NULL) {
    goto fail;
  }

  for(i=0; i<sizeof(calls)/sizeof(int); i++) {
    if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, calls[i], 0) != 0) {
      goto fail;
    }
  }

  if (seccomp_load(ctx) < 0) {
    goto fail;
  }

  ret = 0;
fail:
  if (ctx != NULL) {
    seccomp_release(ctx);
  }

  return ret;
}

#else
#error "Unsupported platform"
#endif
