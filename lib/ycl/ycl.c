#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <lib/util/buf.h>
#include <lib/util/io.h>
#include <lib/util/netstring.h>

#include <lib/ycl/ycl.h>

#define SETERR(ycl, ...) \
  snprintf((ycl)->errbuf, sizeof((ycl)->errbuf), __VA_ARGS__)

#define DFL_MAXMSGSZ (1 << 20)

void ycl_init(struct ycl_ctx *ycl, int fd) {
  ycl->fd = fd;
  ycl->flags = 0;
  ycl->max_msgsz = DFL_MAXMSGSZ;
  ycl->errbuf[0] = '\0';
}

int ycl_connect(struct ycl_ctx *ycl, const char *dst) {
  int ret;
  io_t io;

  ret = io_connect_unix(&io, dst);
  if (ret < 0) {
    SETERR(ycl, "%s", io_strerror(&io));
    return YCL_ERR;
  }

  ycl_init(ycl, IO_FILENO(&io));
  return YCL_OK;
}

int ycl_close(struct ycl_ctx *ycl) {
  int ret;

  if (!(ycl->flags & YCL_EXTERNALFD)) {
    ret = close(ycl->fd);
    if (ret < 0) {
      SETERR(ycl, "close: %s", strerror(errno));
      return YCL_ERR;
    }
  }

  ycl->flags = 0;
  ycl->errbuf[0] = '\0';
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
  char *data;
  size_t left;
  ssize_t ret;

  /* this function is written with re-entrancy for non-blocking I/O in mind
   * , where msg holds the message state. ycl_msg_reset should be called
   * on an init-ed message before each new message passed to this function */

  left = msg->buf.len - msg->sendoff;
  data = msg->buf.data + msg->sendoff;
  while (left > 0) {
    ret = write(ycl->fd, data, left);
    if (ret < 0) {
      if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
        return YCL_AGAIN;
      } else {
        SETERR(ycl, "ycl_sendmsg: %s", strerror(errno));
        return YCL_ERR;
      }
    }

    msg->sendoff += ret;
    data += ret;
    left -= ret;
  }

  /* reset the message buffer after the message is sent so that
   * sendmsg followed by a recvmsg receives from a clean state and not
   * the previously sent message */
  ycl_msg_reset(msg);
  return YCL_OK;
}

int ycl_recvmsg(struct ycl_ctx *ycl, struct ycl_msg *msg) {
  io_t io;
  size_t nread = 0;
  int ret;

  /* check if we have a previous invocation */
  if (msg->nextoff > 0) {
    /* check if the previous invocation has trailing data */
    if (msg->nextoff < msg->buf.len) {
      /* move the trailing data to the start of the message buffer */
      memmove(msg->buf.data, msg->buf.data + msg->nextoff,
          msg->buf.len - msg->nextoff);
      msg->buf.len -= msg->nextoff;
      /* is the previous data a complete message? */
      ret = netstring_tryparse(msg->buf.data, msg->buf.len, &msg->nextoff);
      if (ret == NETSTRING_OK) {
        return YCL_OK;
      } else if (ret != NETSTRING_ERRINCOMPLETE) {
        goto parse_err;
      }
    } else {
      /* no trailing data from the previous invocation; clear buffer */
      buf_clear(&msg->buf);
    }
  }
  msg->nextoff = 0;

  IO_INIT(&io, ycl->fd);
  while (msg->buf.len == 0 ||
      (ret = netstring_tryparse(msg->buf.data, msg->buf.len,
       &msg->nextoff)) == NETSTRING_ERRINCOMPLETE) {
    ret = io_readbuf(&io, &msg->buf, &nread);
    if (ret == IO_AGAIN) {
      return YCL_AGAIN;
    } else if (ret != IO_OK) {
      SETERR(ycl, "%s", io_strerror(&io));
      return YCL_ERR;
    } else if (nread == 0) {
      SETERR(ycl, "premature connection termination");
      return YCL_ERR;
    } else if (msg->buf.len >= ycl->max_msgsz) {
      SETERR(ycl, "message too large");
      return YCL_ERR;
    }
  }

  if (ret != NETSTRING_OK) {
    goto parse_err;
  }

  return YCL_OK;

parse_err:
  SETERR(ycl, "message parse error: %s", netstring_strerror(ret));
  return YCL_ERR;
}

int ycl_recvfd(struct ycl_ctx *ycl, int *fd) {
  io_t io;
  int ret;

  IO_INIT(&io, ycl->fd);
  ret = io_recvfd(&io, fd);
  if (ret == IO_AGAIN) {
    return YCL_AGAIN;
  } else if (ret != IO_OK) {
    SETERR(ycl, "%s", io_strerror(&io));
    return YCL_ERR;
  }

  return YCL_OK;
}

int ycl_sendfd(struct ycl_ctx *ycl, int fd, int err) {
  io_t io;
  int ret;

  IO_INIT(&io, ycl->fd);
  ret = io_sendfd(&io, fd, err);
  if (ret == IO_AGAIN) {
    return YCL_AGAIN;
  } else if (ret != IO_OK) {
    SETERR(ycl, "%s", io_strerror(&io));
    return YCL_ERR;
  }

  return YCL_OK;
}

int ycl_msg_init(struct ycl_msg *msg) {
  if (buf_init(&msg->buf, 1024) == NULL) {
    return YCL_ERR;
  }

  if (buf_init(&msg->mbuf, 1024) == NULL) {
    buf_cleanup(&msg->buf);
    return YCL_ERR;
  }

  msg->sendoff = 0;
  msg->nextoff = 0;
  msg->flags = 0;
  return YCL_OK;
}

int ycl_msg_set(struct ycl_msg *msg, const void *data, size_t len) {
  ycl_msg_reset(msg);
  if (buf_adata(&msg->buf, data, len) < 0) {
    return YCL_ERR;
  }
  return YCL_OK;
}

int ycl_msg_use_optbuf(struct ycl_msg *msg) {
  if (!(msg->flags & YCLMSGF_HASOPTBUF)) {
    if (buf_init(&msg->optbuf, 1024) == NULL) {
      return YCL_ERR;
    }
    msg->flags |= YCLMSGF_HASOPTBUF;
  }
  return YCL_OK;
}

void ycl_msg_reset(struct ycl_msg *msg) {
  buf_clear(&msg->buf);
  buf_clear(&msg->mbuf);
  if (msg->flags & YCLMSGF_HASOPTBUF) {
    buf_clear(&msg->optbuf);
  }
  msg->sendoff = 0;
  msg->nextoff = 0;
}

void ycl_msg_cleanup(struct ycl_msg *msg) {
  buf_cleanup(&msg->buf);
  buf_cleanup(&msg->mbuf);
  if (msg->flags & YCLMSGF_HASOPTBUF) {
    buf_cleanup(&msg->optbuf);
  }
}
