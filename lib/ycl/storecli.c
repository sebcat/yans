#include <string.h>

#include <lib/ycl/storecli.h>

#define storecli_seterr(ctx__, err__) ((ctx__)->err = (err__))

int storecli_enter(struct storecli_ctx *ctx, const char *id,
    const char *name, long indexed) {
  struct ycl_msg_store_req reqmsg = {{0}};
  struct ycl_msg_status_resp respmsg = {{0}};
  int ret;

  reqmsg.action.data = "enter";
  reqmsg.action.len = sizeof("enter") - 1;
  reqmsg.store_id.data = id;
  reqmsg.store_id.len = id ? strlen(id) : 0;
  reqmsg.name.data = name;
  reqmsg.name.len = name ? strlen(name) : 0;
  reqmsg.indexed = indexed;
  ret = ycl_msg_create_store_req(ctx->msgbuf, &reqmsg);
  if (ret != YCL_OK) {
    storecli_seterr(ctx, "ycl_msg_create_store_enter failure");
    return STORECLI_ERR;
  }

  ret = ycl_sendmsg(ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    storecli_seterr(ctx, ycl_strerror(ctx->ycl));
    return STORECLI_ERR;
  }

  ycl_msg_reset(ctx->msgbuf);
  ret = ycl_recvmsg(ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    storecli_seterr(ctx, ycl_strerror(ctx->ycl));
    return STORECLI_ERR;
  }

  ret = ycl_msg_parse_status_resp(ctx->msgbuf, &respmsg);
  if (ret != YCL_OK) {
    storecli_seterr(ctx, "failed to parse enter response");
    return STORECLI_ERR;
  }

  if (respmsg.errmsg.data != NULL && *respmsg.errmsg.data != '\0') {
    storecli_seterr(ctx, respmsg.errmsg.data);
    return STORECLI_ERR;
  }

  if (respmsg.okmsg.data != NULL && *respmsg.okmsg.data != '\0') {
    strncpy(ctx->entered_id, respmsg.okmsg.data, sizeof(ctx->entered_id));
    ctx->entered_id[sizeof(ctx->entered_id)-1] = '\0';
  }

  return STORECLI_OK;
}

int storecli_open(struct storecli_ctx *ctx, const char *path, int flags,
    int *outfd) {
  struct ycl_msg_store_entered_req openmsg = {{0}};
  int ret;

  openmsg.action.data = "open";
  openmsg.action.len = 5;
  openmsg.open_path.data = path;
  openmsg.open_path.len = strlen(path);
  openmsg.open_flags = flags;
  ret = ycl_msg_create_store_entered_req(ctx->msgbuf, &openmsg);
  if (ret != YCL_OK) {
    storecli_seterr(ctx, "failed to serialize open request\n");
    return STORECLI_ERR;
  }

  ret = ycl_sendmsg(ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    storecli_seterr(ctx, ycl_strerror(ctx->ycl));
    return STORECLI_ERR;
  }

  ret = ycl_recvfd(ctx->ycl, outfd);
  if (ret != YCL_OK) {
    storecli_seterr(ctx, ycl_strerror(ctx->ycl));
    return STORECLI_ERR;
  }

  return STORECLI_OK;
}
