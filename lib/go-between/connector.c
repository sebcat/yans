#include <string.h>
#include <errno.h>

#include <lib/ycl/ycl_msg.h>
#include <lib/go-between/connector.h>

#define CONNECTORF_IS_YCL    (1 << 0) /* ycl backend is being used */
#define CONNECTORF_IS_TCPSRC (1 << 1) /* tcpsrc(4) backend is being used */

static int connector_init_svc(struct connector_ctx *ctx,
    struct connector_opts *opts) {
  int ret;

  ret = ycl_connect(&ctx->ycl, opts->svcpath);
  if (ret != YCL_OK) {
    return -1;
  }

  return 0;
}

int connector_init(struct connector_ctx *ctx,
    struct connector_opts *opts) {
  int ret;

  memset(ctx, 0, sizeof(*ctx));
  if (opts->use_tcpsrc) {
    ret = tcpsrc_init(&ctx->tcpsrc);
    if (ret < 0) {
      ctx->err = errno;
      return -1;
    }

    ctx->flags |= CONNECTORF_IS_TCPSRC;
  } else if (opts->svcpath && *opts->svcpath) {
    if (!opts->msgbuf) {
      /* svc path without supplied YCL msg buffer is not valid ATM */
      ctx->err = EINVAL;
      return -1;
    }

    ctx->flags |= CONNECTORF_IS_YCL;
    ctx->msgbuf = opts->msgbuf;
    ret = connector_init_svc(ctx, opts);
    if (ret < 0) {
      return -1;
    }
  }

  return 0;
}

void connector_cleanup(struct connector_ctx *ctx) {
  if (ctx->flags & CONNECTORF_IS_YCL) {
    ycl_close(&ctx->ycl);
    ctx->flags &= ~CONNECTORF_IS_YCL;
  }
}

static int connector_connect_svc(struct connector_ctx *ctx,
    struct sconn_opts *opts) {
  int ret;
  int getfd = -1;
  struct ycl_msg_connector_req req;

  req.reuse_addr = opts->reuse_addr;
  req.proto = opts->proto;
  req.bindaddr.data = (const char *)opts->bindaddr;
  req.bindaddr.len = opts->bindaddr ? opts->bindaddrlen : 0;
  req.dstaddr.data = (const char *)opts->dstaddr;
  req.dstaddr.len = opts->dstaddr ? opts->dstaddrlen : 0;
  ret = ycl_msg_create_connector_req(ctx->msgbuf, &req);
  if (ret != YCL_OK) {
    return -1; /* TODO: propagate error - not in ycl context */
  }

  ret = ycl_sendmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return -1;
  }

  ret = ycl_recvfd(&ctx->ycl, &getfd);
  if (ret != YCL_OK) {
    return -1;
  }

  return getfd;
}

static int connector_connect_tcpsrc(struct connector_ctx *ctx,
    struct sconn_opts *opts) {
  int ret;

  if (opts->proto != IPPROTO_TCP) {
    ctx->err = EINVAL;
    return -1;
  }

  /* TODO: Implement a lot of the other functionality in tcpsrc(4)
   * (e.g., reuseaddr) */

  ret = tcpsrc_connect(&ctx->tcpsrc, opts->dstaddr);
  if (ret < 0) {
    ctx->err = errno;
  }

  return ret;
}

static int connector_connect_sconn(struct connector_ctx *ctx,
    struct sconn_opts *opts) {
  int ret;

  ret = sconn_connect(&ctx->sconn, opts);
  if (ret < 0) {
    ctx->err = sconn_errno(&ctx->sconn);
    return -1;
  }

  return ret;
}

int connector_connect(struct connector_ctx *ctx, struct sconn_opts *opts) {
  if (ctx->flags & CONNECTORF_IS_YCL) {
    return connector_connect_svc(ctx, opts);
  } else if (ctx->flags & CONNECTORF_IS_TCPSRC) {
    return connector_connect_tcpsrc(ctx, opts);
  } else {
    return connector_connect_sconn(ctx, opts);
  }
}

const char *connector_strerror(struct connector_ctx *ctx) {
  if (ctx->flags & CONNECTORF_IS_YCL) {
    return ycl_strerror(&ctx->ycl);
  } else {
    return strerror(ctx->err);
  }
}

