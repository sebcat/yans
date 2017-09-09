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

/* the reason this is here, is because the build system is/should be
 * using -fvisibility=hidden, except for library code */
#pragma GCC visibility push(default)

void ycl_init(struct ycl_ctx *ycl, int fd) {
  ycl->fd = fd;
  ycl->flags = 0;
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

  left = msg->buf.len - msg->off;
  data = msg->buf.data + msg->off;
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

    msg->off += ret;
    data += ret;
    left -= ret;
  }

  return YCL_OK;
}

int ycl_recvmsg(struct ycl_ctx *ycl, struct ycl_msg *msg) {
  io_t io;
  size_t nread = 0;
  int ret;

  /* this function is written with re-entrancy for non-blocking I/O in mind
   * , where msg holds the message state. ycl_msg_reset should be called
   * on an init-ed message before each new message passed to this function */

  IO_INIT(&io, ycl->fd);
  while (msg->buf.len == 0 ||
      (ret = netstring_tryparse(msg->buf.data, msg->buf.len)) ==
      NETSTRING_ERRINCOMPLETE) {
    ret = io_readbuf(&io, &msg->buf, &nread);
    if (ret == IO_AGAIN) {
      return YCL_AGAIN;
    } else if (ret != IO_OK) {
      SETERR(ycl, "%s", io_strerror(&io));
      return YCL_ERR;
    } else if (nread == 0) {
      SETERR(ycl, "connection terminated prematurely");
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
  if (buf_init(&msg->buf, 1024) == NULL) {
    return YCL_ERR;
  }

  return YCL_OK;
}

void ycl_msg_reset(struct ycl_msg *msg) {
  buf_clear(&msg->buf);
  msg->off = 0;
}

void ycl_msg_cleanup(struct ycl_msg *msg) {
  buf_cleanup(&msg->buf);
}

int ycl_msg_create_pcap_req(struct ycl_msg *msg, const char *iface,
    const char *filter) {
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
  if (p_pcap_req_serialize(&req, &msg->buf) != PROTO_OK) {
    return YCL_ERR;
  }

  return YCL_OK;
}

int ycl_msg_create_pcap_close(struct ycl_msg *msg) {
  ycl_msg_reset(msg);
  if (buf_adata(&msg->buf, "0:,", 3) < 0) {
    return YCL_ERR;
  }
  return YCL_OK;
}

int ycl_msg_create_ethframe_req(struct ycl_msg *msg,
    struct ycl_ethframe_req *framereq) {
  struct p_ethframe_req req = {0};
  buf_t tmpbuf = {0};

  if (framereq->ncustom_frames > 0) {
    if (buf_init(&tmpbuf, 2048) == NULL) {
      goto fail;
    }

    if (netstring_append_list(&tmpbuf, framereq->ncustom_frames,
        framereq->custom_frames, framereq->custom_frameslen) != NETSTRING_OK) {
      goto fail;
    }

    req.custom_frames = tmpbuf.data;
    req.custom_frameslen = tmpbuf.len;
  }

  if (framereq->categories != NULL) {
    req.categories = framereq->categories;
    req.categorieslen = strlen(framereq->categories);
  }

  if (framereq->iface != NULL) {
    req.iface = framereq->iface;
    req.ifacelen = strlen(framereq->iface);
  }

  if (framereq->pps != NULL) {
    req.pps = framereq->pps;
    req.ppslen = strlen(framereq->pps);
  }

  if (framereq->eth_src != NULL) {
    req.eth_src = framereq->eth_src;
    req.eth_srclen = strlen(framereq->eth_src);
  }

  if (framereq->eth_dst != NULL) {
    req.eth_dst = framereq->eth_dst;
    req.eth_dstlen = strlen(framereq->eth_dst);
  }

  if (framereq->ip_src != NULL) {
    req.ip_src = framereq->ip_src;
    req.ip_srclen = strlen(framereq->ip_src);
  }

  if (framereq->ip_dsts != NULL) {
    req.ip_dsts = framereq->ip_dsts;
    req.ip_dstslen = strlen(framereq->ip_dsts);
  }

  if (framereq->port_dsts != NULL) {
    req.port_dsts = framereq->port_dsts;
    req.port_dstslen = strlen(framereq->port_dsts);
  }

  ycl_msg_reset(msg);
  if (p_ethframe_req_serialize(&req, &msg->buf) != PROTO_OK) {
    goto fail;
  }

  buf_cleanup(&tmpbuf);
  return YCL_OK;

fail:
  buf_cleanup(&tmpbuf);
  return YCL_ERR;
}

int ycl_msg_create_status_resp(struct ycl_msg *msg,
    struct ycl_status_resp *r) {
  struct p_status_resp resp = {0};

  if (r->okmsg != NULL) {
    resp.okmsg = r->okmsg;
    resp.okmsglen = strlen(r->okmsg);
  }

  if (r->errmsg != NULL) {
    resp.errmsg = r->errmsg;
    resp.errmsglen = strlen(resp.errmsg);
  }

  ycl_msg_reset(msg);
  if (p_status_resp_serialize(&resp, &msg->buf) != PROTO_OK) {
    return YCL_ERR;
  }

  return YCL_OK;
}

int ycl_msg_parse_status_resp(struct ycl_msg *msg, struct ycl_status_resp *r) {
  struct p_status_resp resp = {0};

  if (p_status_resp_deserialize(&resp, msg->buf.data, msg->buf.len, NULL) !=
      PROTO_OK) {
    return YCL_ERR;
  }

  if (resp.okmsg != NULL) {
    r->okmsg = resp.okmsg;
  }

  if (resp.errmsg != NULL) {
    r->errmsg = resp.errmsg;
  }

  return YCL_OK;
}

#pragma GCC visibility pop
