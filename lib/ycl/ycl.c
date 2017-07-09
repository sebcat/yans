#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <lib/util/buf.h>
#include <lib/util/io.h>
#include <lib/util/netstring.h>

#include <proto/pcap_req.h>
#include <proto/ethframe_req.h>
#include <proto/status_resp.h>

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

  /* this function is written with re-entrancy for non-blocking I/O in mind
   * , where msg holds the message state. ycl_msg_reset should be called
   * on an init-ed message before each new message passed to this function */

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

  /* this function is written with re-entrancy for non-blocking I/O in mind
   * , where msg holds the message state. ycl_msg_reset should be called
   * on an init-ed message before each new message passed to this function */

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

int ycl_sendfd(struct ycl_ctx *ycl, int fd) {
  io_t io;
  int ret;

  IO_INIT(&io, ycl->fd);
  ret = io_sendfd(&io, fd);
  if (ret != IO_OK) {
    SETERR(ycl, "%s", io_strerror(&io));
    return YCL_ERR;
  }

  return YCL_OK;
}

int ycl_msg_init(struct ycl_msg *msg) {
  struct ycl_msg_internal *m = YCL_MSG_INTERNAL(msg);
  if (buf_init(&m->buf, 1024) == NULL) {
    return YCL_ERR;
  }

  return YCL_OK;
}

void ycl_msg_reset(struct ycl_msg *msg) {
  struct ycl_msg_internal *m = YCL_MSG_INTERNAL(msg);
  buf_clear(&m->buf);
  m->off = 0;
}

void ycl_msg_cleanup(struct ycl_msg *msg) {
  struct ycl_msg_internal *m = YCL_MSG_INTERNAL(msg);
  buf_cleanup(&m->buf);
}

int ycl_msg_create_pcap_req(struct ycl_msg *msg, const char *iface,
    const char *filter) {
  struct ycl_msg_internal *m = YCL_MSG_INTERNAL(msg);
  struct p_pcap_req req = {0};

  if (iface != NULL) {
    req.iface = iface;
    req.ifacelen = strlen(iface);
  }

  if (filter != NULL) {
    req.filter = filter;
    req.filterlen = strlen(filter);
  }

  ycl_msg_reset(msg);
  if (p_pcap_req_serialize(&req, &m->buf) != PROTO_OK) {
    return YCL_ERR;
  }

  return YCL_OK;
}

int ycl_msg_create_ethframe_req(struct ycl_msg *msg, const char *iface,
    size_t nframes, const char **frames, size_t *frameslen) {
  struct ycl_msg_internal *m = YCL_MSG_INTERNAL(msg);
  struct p_ethframe_req req = {0};
  int ret;
  buf_t tmpbuf;

  if (buf_init(&tmpbuf, 2048) == NULL) {
    return YCL_ERR;
  } else if (netstring_append_list(&tmpbuf, nframes, frames, frameslen) !=
      NETSTRING_OK) {
    return YCL_ERR;
  }

  req.frames = tmpbuf.data;
  req.frameslen = tmpbuf.len;
  if (iface != NULL) {
    req.iface = iface;
    req.ifacelen = strlen(iface);
  }

  ycl_msg_reset(msg);
  ret = p_ethframe_req_serialize(&req, &m->buf);
  buf_cleanup(&tmpbuf);
  if (ret != PROTO_OK) {
    return YCL_ERR;
  }

  return YCL_OK;
}

int ycl_msg_create_pcap_close(struct ycl_msg *msg) {
  struct ycl_msg_internal *m = YCL_MSG_INTERNAL(msg);
  ycl_msg_reset(msg);
  if (buf_adata(&m->buf, "0:,", 3) < 0) {
    return YCL_ERR;
  }
  return YCL_OK;
}

int ycl_msg_parse_status_resp(struct ycl_msg *msg, const char **okmsg,
    const char **errmsg) {
  struct ycl_msg_internal *m = YCL_MSG_INTERNAL(msg);
  struct p_status_resp resp = {0};

  if (p_status_resp_deserialize(&resp, m->buf.data, m->buf.len, NULL) !=
      PROTO_OK) {
    return YCL_ERR;
  }

  if (okmsg != NULL) {
    *okmsg = resp.okmsg;
  }

  if (errmsg != NULL) {
    *errmsg = resp.errmsg;
  }

  return YCL_OK;
}

#pragma GCC visibility pop
