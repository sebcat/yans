#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <lib/net/tcpsrc.h>

#define TCPSRC_PATH "/dev/tcpsrc"

int tcpsrc_fdinit(struct tcpsrc_ctx *ctx, int fd) {
  ctx->fd = fd;
  return 0;
}

int tcpsrc_init(struct tcpsrc_ctx *ctx) {
  int fd;

  fd = open(TCPSRC_PATH, O_RDWR);
  if (fd < 0) {
    return -1;
  }

  return tcpsrc_fdinit(ctx, fd);
}

void tcpsrc_cleanup(struct tcpsrc_ctx *ctx) {
  close(ctx->fd);
  ctx->fd = -1;
}

int tcpsrc_connect(struct tcpsrc_ctx *ctx, struct sockaddr *sa) {
  struct tcpsrc_conn c = {{{0}}};

  switch (sa->sa_family) {
  case AF_INET:
    memcpy(&c.u.sin, sa, sizeof(struct sockaddr_in));
    break;
  case AF_INET6:
    memcpy(&c.u.sin6, sa, sizeof(struct sockaddr_in6));
    break;
  default:
    errno = EINVAL;
    return -1;
  }

  return ioctl(ctx->fd, TCPSRC_CONNECT, &c);
}

