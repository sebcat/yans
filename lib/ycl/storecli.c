#include <string.h>

#include <lib/ycl/ycl_msg.h>
#include <lib/ycl/storecli.h>

static inline int storecli_seterr(struct storecli_ctx *ctx, const char *err) {
  ctx->err = err;
  return YCL_ERR;
}

void storecli_init(struct storecli_ctx *ctx, struct ycl_msg *msgbuf) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->msgbuf = msgbuf;
}

int storecli_connect(struct storecli_ctx *ctx, const char *path) {
  int ret;

  ret = ycl_connect(&ctx->ycl, path);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  return YCL_OK;
}

int storecli_close(struct storecli_ctx *ctx) {
  int ret;

  ret = ycl_close(&ctx->ycl);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  return YCL_OK;
}

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
    return storecli_seterr(ctx, "ycl_msg_create_store_enter failure");
  }

  ret = ycl_sendmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ycl_msg_reset(ctx->msgbuf);
  ret = ycl_recvmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ret = ycl_msg_parse_status_resp(ctx->msgbuf, &respmsg);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, "failed to parse enter response");
  }

  if (respmsg.errmsg.data != NULL && *respmsg.errmsg.data != '\0') {
    return storecli_seterr(ctx, respmsg.errmsg.data);
  }

  if (respmsg.okmsg.data != NULL && *respmsg.okmsg.data != '\0') {
    strncpy(ctx->entered_id, respmsg.okmsg.data, sizeof(ctx->entered_id));
    ctx->entered_id[sizeof(ctx->entered_id)-1] = '\0';
  }

  return YCL_OK;
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
    return storecli_seterr(ctx, "failed to serialize open request\n");
  }

  ret = ycl_sendmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ret = ycl_recvfd(&ctx->ycl, outfd);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  return YCL_OK;
}

int storecli_index(struct storecli_ctx *ctx, size_t before, size_t nelems,
    int *outfd) {
  struct ycl_msg_store_req reqmsg = {{0}};
  int ret;

  reqmsg.action.data = "index";
  reqmsg.action.len = sizeof("index") - 1;
  ret = ycl_msg_create_store_req(ctx->msgbuf, &reqmsg);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, "ycl_msg_create_store_enter failure");
  }

  ret = ycl_sendmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ycl_msg_reset(ctx->msgbuf);
  ret = ycl_recvfd(&ctx->ycl, outfd);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  return YCL_OK;
}

int storecli_list(struct storecli_ctx *ctx, const char *id,
    const char *must_match, const char **result, size_t *resultlen) {
  struct ycl_msg_store_req reqmsg = {{0}};
  struct ycl_msg_store_list respmsg = {{0}};
  int ret;

  reqmsg.action.data = "list";
  reqmsg.action.len = sizeof("list") - 1;
  reqmsg.store_id.data = id;
  reqmsg.store_id.len = id ? strlen(id) : 0;
  reqmsg.list_must_match.data = must_match;
  reqmsg.list_must_match.len = must_match ? strlen(must_match) : 0;
  ret = ycl_msg_create_store_req(ctx->msgbuf, &reqmsg);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, "ycl_msg_create_store_req failure");
  }

  ret = ycl_sendmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ycl_msg_reset(ctx->msgbuf);
  ret = ycl_recvmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ret = ycl_msg_parse_store_list(ctx->msgbuf, &respmsg);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, "failed to parse store list response");
  }

  if (respmsg.errmsg.len > 0) {
    return storecli_seterr(ctx, respmsg.errmsg.data);
  }

  *result = respmsg.entries.data;
  *resultlen = respmsg.entries.len;
  return YCL_OK;
}

int storecli_rename(struct storecli_ctx *ctx, const char *from,
    const char *to) {
  struct ycl_msg_store_entered_req renamemsg = {{0}};
  struct ycl_msg_status_resp respmsg = {{0}};
  int ret;

  renamemsg.action.data = "rename";
  renamemsg.action.len = 7;
  renamemsg.rename_from.data = from;
  renamemsg.rename_from.len = from ? strlen(from) : 0;
  renamemsg.rename_to.data = to;
  renamemsg.rename_to.len = to ? strlen(to) : 0;
  ret = ycl_msg_create_store_entered_req(ctx->msgbuf, &renamemsg);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, "failed to serialize rename request");
  }

  ret = ycl_sendmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ycl_msg_reset(ctx->msgbuf);
  ret = ycl_recvmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ret = ycl_msg_parse_status_resp(ctx->msgbuf, &respmsg);
  if (ret != YCL_OK) {
    return storecli_seterr(ctx, "failed to parse rename response\n");
  }

  if (respmsg.errmsg.data != NULL && *respmsg.errmsg.data != '\0') {
    return storecli_seterr(ctx, respmsg.errmsg.data);
  }

  return YCL_OK;
}
