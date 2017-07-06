#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <lib/util/buf.h>
#include <lib/util/io.h>
#include <lib/util/netstring.h>

#include <lib/ycl/ycl.h>

#define SETERR(ycl, ...) \
  snprintf((ycl)->errbuf, sizeof((ycl)->errbuf), __VA_ARGS__)

#pragma GCC visibility push(default)

struct ycl_msg_internal {
  buf_t buf;
  size_t off;
};

#define YCL_MSG_INTERNAL(msg) \
    (struct ycl_msg_internal*)(msg)

int ycl_connect(struct ycl_ctx *ycl, const char *dst) {
  int ret;
  io_t io;

   _Static_assert(sizeof(struct ycl_msg_internal) <= YCL_IDATASIZ,
       "YCL_IDATASIZ is too small");
  ret = io_connect_unix(&io, dst);
  if (ret < 0) {
    SETERR(ycl, "%s", io_strerror(&io));
    return YCL_ERR;
  }

  ycl->fd = IO_FILENO(&io);
  ycl->errbuf[0] = '\0';
  return YCL_OK;
}

int ycl_close(struct ycl_ctx *ycl) {
  int ret;

  ret = close(ycl->fd);
  if (ret < 0) {
    SETERR(ycl, "close: %s", strerror(errno));
    return YCL_ERR;
  }
  ycl->fd = -1;
  return YCL_OK;
}

int ycl_setnonblock(struct ycl_ctx *ycl, int status) {
  io_t io;
  int ret;

  IO_INIT(&io, ycl->fd);
  ret = io_setnonblock(&io, status);
  if (ret < 0) {
    SETERR(ycl, "%s", io_strerror(&io));
    return YCL_ERR;
  }

  return YCL_OK;
}

int ycl_sendmsg(struct ycl_ctx *ycl, struct ycl_msg *msg) {
  struct ycl_msg_internal *m = YCL_MSG_INTERNAL(msg);
  char *data;
  size_t left;
  ssize_t ret;

  left = m->buf.len - m->off;
  data = m->buf.data + m->off;
  while (left > 0) {
    ret = write(ycl->fd, data, left);
    if (ret < 0) {
      if (errno == EINTR) {
        continue;
      } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
        return YCL_AGAIN;
      } else {
        SETERR(ycl, "ycl_sendmsg: %s", strerror(errno));
        return YCL_ERR;
      }
    }

    m->off += ret;
    data += ret;
    left -= ret;
  }

  return YCL_OK;
}

int ycl_recvmsg(struct ycl_ctx *ycl, struct ycl_msg *msg) {
  struct ycl_msg_internal *m = YCL_MSG_INTERNAL(msg);
  io_t io;
  int ret;

  IO_INIT(&io, ycl->fd);
  while (m->buf.len == 0 ||
      (ret = netstring_tryparse(m->buf.data, m->buf.len)) ==
      NETSTRING_ERRINCOMPLETE) {
    ret = io_readbuf(&io, &m->buf, NULL);
    if (ret == IO_AGAIN) {
      return YCL_AGAIN;
    } else if (ret != IO_OK) {
      SETERR(ycl, "%s", io_strerror(&io));
      return YCL_ERR;
    }
  }

  if (ret != NETSTRING_OK) {
    SETERR(ycl, "message parse error: %s", netstring_strerror(ret));
    return YCL_ERR;
  }

  return YCL_OK;
}

#pragma GCC visibility pop
