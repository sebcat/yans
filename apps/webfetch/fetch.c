#include <string.h>

#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>
#include <apps/webfetch/fetch.h>

#if 0
static int load_http_msg(struct ycl_ctx *ycl, struct ycl_msg *msgbuf,
    struct ycl_msg_httpmsg *httpmsg, FILE *infp) {
  int ret;
  int status = -1;

  ret = ycl_readmsg(ycl, msgbuf, infp);
  if (ret != YCL_OK) {
    goto done;
  }

  ret = ycl_msg_parse_httpmsg(msgbuf, httpmsg);
  if (ret != YCL_OK) {
    goto done;
  }

  status = 0;
done:
  return status;
}
#endif

int fetch_init(struct fetch_ctx *ctx, struct fetch_opts *opts) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->opts = *opts;
  return 0; /* TODO: Implement */
}

void fetch_cleanup(struct fetch_ctx *ctx) {
  /* TODO: Implement */
}

int fetch_run(struct fetch_ctx *ctx) {
  return 0; /* TODO: Implement */
}
