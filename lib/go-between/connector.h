#ifndef YANS_OPENER_H__
#define YANS_OPENER_H__

#include <lib/net/tcpsrc.h>
#include <lib/net/sconn.h>
#include <lib/ycl/ycl.h>

struct connector_opts {
  const char *svcpath;    /* optional, socket path to clid(1) daemon */
  struct ycl_msg *msgbuf; /* required if svcpath is set */
  int use_tcpsrc;         /* if 1, use tcpsrc(4) backend */
};

struct connector_ctx {
  /* internal */
  int flags;
  int err;
  struct ycl_ctx ycl;
  struct ycl_msg *msgbuf;
  struct sconn_ctx sconn;
  struct tcpsrc_ctx tcpsrc;
};

int connector_init(struct connector_ctx *ctx, struct connector_opts *opts);
void connector_cleanup(struct connector_ctx *ctx);

int connector_connect(struct connector_ctx *ctx, struct sconn_opts *opts);
const char *connector_strerror(struct connector_ctx *ctx);

#endif
