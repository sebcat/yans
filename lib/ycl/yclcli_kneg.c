#include <string.h>

#include <lib/ycl/yclcli_kneg.h>
#include <lib/ycl/ycl_msg.h>

static int reqresp(struct yclcli_ctx *ctx, struct ycl_msg_knegd_req *req,
    char **out) {
  struct ycl_msg_status_resp resp = {{0}};
  int ret;

  ret = ycl_msg_create_knegd_req(ctx->msgbuf, req);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, "failed to create knegd request");
  }

  ret = ycl_sendmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ycl_msg_reset(ctx->msgbuf);
  ret = ycl_recvmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ret = ycl_msg_parse_status_resp(ctx->msgbuf, &resp);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, "failed to parse status response");
  }

  if (resp.errmsg.data != NULL) {
    return yclcli_seterr(ctx, resp.errmsg.data);
  }

  if (out) {
    *out = (char*)resp.okmsg.data;
  }

  return YCL_OK;
}

int yclcli_kneg_manifest(struct yclcli_ctx *ctx, char **out) {
  struct ycl_msg_knegd_req req = {0};

  req.action.data = "manifest";
  req.action.len = sizeof("manifest")-1;
  return reqresp(ctx, &req, out);
}

int yclcli_kneg_queueinfo(struct yclcli_ctx *ctx, char **out) {
  struct ycl_msg_knegd_req req = {0};

  req.action.data = "queueinfo";
  req.action.len = sizeof("queueinfo")-1;
  return reqresp(ctx, &req, out);
}

int yclcli_kneg_status(struct yclcli_ctx *ctx, const char *id,
    size_t idlen, char **out) {
  struct ycl_msg_knegd_req req = {0};

  req.action.data = "status";
  req.action.len = sizeof("status")-1;
  req.id.data = id;
  req.id.len = idlen;
  return reqresp(ctx, &req, out);
}

int yclcli_kneg_queue(struct yclcli_ctx *ctx, const char *id,
    const char *type, const char *name, char **out) {
  struct ycl_msg_knegd_req req = {
    .action = {
      .data = "queue",
      .len = sizeof("queue") -1,
    },
    .id = {
      .data = id,
      .len = strlen(id),
    },
    .type = {
      .data = type,
      .len = strlen(type),
    },
    .name = {
      .data = name,
      .len = strlen(name),
    },
  };

  return reqresp(ctx, &req, out);
}
