#include <string.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>

#include <lib/ycl/resolvercli.h>

static int resolvercli_error(struct resolvercli_ctx *ctx, const char *err) {
  ctx->err = err;
  return RESOLVERCLI_ERR;
}

int resolvercli_resolve(struct resolvercli_ctx *ctx, int dstfd,
    const char *spec, size_t speclen, int compress) {
  struct ycl_msg_resolver_req req = {{0}};
  int ret;
  int closefd = -1;
  struct pollfd pfd;

  req.hosts.data = spec;
  req.hosts.len = speclen;
  req.compress = compress;
  ret = ycl_msg_create_resolver_req(ctx->msgbuf, &req);
  if (ret != YCL_OK) {
    return resolvercli_error(ctx, "failed to serialize resolver request");
  }

  ret = ycl_sendfd(ctx->ycl, dstfd, 0);
  if (ret != YCL_OK) {
    return resolvercli_error(ctx, ycl_strerror(ctx->ycl));
  }

  ret = ycl_sendmsg(ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return resolvercli_error(ctx, ycl_strerror(ctx->ycl));
  }

  ret = ycl_recvfd(ctx->ycl, &closefd);
  if (ret != YCL_OK) {
    return resolvercli_error(ctx, ycl_strerror(ctx->ycl));
  }

  /* wait syncronously for completion */
  pfd.fd = closefd;
  pfd.events = POLLIN | POLLPRI;
  pfd.revents = 0;
  do {
    ret = poll(&pfd, 1, -1);
  } while (ret < 0 && errno == EINTR);
  if (ret < 0) {
    close(closefd);
    return resolvercli_error(ctx, strerror(errno));
  }

  close(closefd);
  return RESOLVERCLI_OK;
}
