#include <string.h>

#include <lib/ycl/yclcli.h>

void yclcli_init(struct yclcli_ctx *ctx, struct ycl_msg *msgbuf) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->msgbuf = msgbuf;
}

int yclcli_connect(struct yclcli_ctx *ctx, const char *path) {
  int ret;

  ret = ycl_connect(&ctx->ycl, path);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  return YCL_OK;
}

int yclcli_close(struct yclcli_ctx *ctx) {
  int ret;

  ret = ycl_close(&ctx->ycl);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  return YCL_OK;
}

