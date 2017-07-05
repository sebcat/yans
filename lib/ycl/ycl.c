#include <lib/util/io.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <lib/ycl/ycl.h>

#define SETERR(ycl, ...) \
  snprintf((ycl)->errbuf, sizeof((ycl)->errbuf), __VA_ARGS__)

#pragma GCC visibility push(default)

int ycl_connect(struct ycl_ctx *ycl, const char *dst) {
  int ret;
  io_t io;

  ret = io_connect_unix(&io, dst);
  if (ret < 0) {
    SETERR(ycl, "%s", io_strerror(&io));
    return -1;
  }

  ycl->fd = IO_FILENO(&io);
  ycl->errbuf[0] = '\0';
  return 0;
}

int ycl_close(struct ycl_ctx *ycl) {
  int ret;

  ret = close(ycl->fd);
  if (ret < 0) {
    SETERR(ycl, "close: %s", strerror(errno));
    return -1;
  }
  ycl->fd = -1;
  return 0;
}

int ycl_setnonblock(struct ycl_ctx *ycl, int status) {
  io_t io;
  int ret;

  IO_INIT(&io, ycl->fd);
  ret = io_setnonblock(&io, status);
  if (ret < 0) {
    SETERR(ycl, "%s", io_strerror(&io));
    return -1;
  }

  return 0;
}

#pragma GCC visibility pop
