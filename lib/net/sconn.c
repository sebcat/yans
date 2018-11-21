#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lib/net/sconn.h>

#define AF_IS_SUPPORTED(af__) \
    ((af__) == AF_INET || (af__) == AF_INET6)

int sconn_parse_addr(struct sconn_ctx *ctx, const char *addr,
    const char *port, struct sockaddr *dst, socklen_t dstlen) {
  int ret;
  struct addrinfo *addrs;
  struct addrinfo *curr;
  struct addrinfo hints = {
    .ai_family   = AF_UNSPEC,
    .ai_flags    = AI_NUMERICHOST | AI_NUMERICSERV,
  };
  socklen_t actual_len = 0;

  ret = getaddrinfo(addr, port, &hints, &addrs);
  if (ret != 0) {
    goto out;
  }

  for (curr = addrs; curr != NULL; curr = curr->ai_next) {
    if (AF_IS_SUPPORTED(curr->ai_family) && curr->ai_addrlen <= dstlen) {
      memcpy(dst, curr->ai_addr, curr->ai_addrlen);
      actual_len = curr->ai_addrlen;
      break;
    }
  }

  freeaddrinfo(addrs);
out:
  if (actual_len == 0) {
    ctx->errcode = EINVAL;
    return -1;
  }

  return actual_len;
}

int sconn_connect(struct sconn_ctx *ctx, struct sconn_opts *opts) {
  int socktype;
  int family;
  int fd;
  int ret;
  int reuse = 1;

  ctx->errcode = 0; /* clear previous errors, if any */

  family = opts->dstaddrlen == sizeof(struct sockaddr_in) ?
      AF_INET : AF_INET6;
  socktype = opts->proto == IPPROTO_TCP ? SOCK_STREAM : SOCK_DGRAM;
  fd = socket(family, socktype | SOCK_NONBLOCK, opts->proto);
  if (fd < 0) {
    ctx->errcode = errno;
    return -1;
  }

  if (opts->reuse_addr) {
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  }

  if (opts->bindaddr != NULL && opts->bindaddrlen > 0) {
    ret = bind(fd, opts->bindaddr, opts->bindaddrlen);
    if (ret < 0) {
      close(fd);
      ctx->errcode = errno;
      return -1;
    }
  }

  ret = connect(fd, opts->dstaddr, opts->dstaddrlen);
  if (ret < 0 && errno != EINPROGRESS && errno != EINTR) {
    /* EINTR because man says:
     *   "The connection will be established in the background, as in the
     *    case of EINPROGRESS"
     */
    close(fd);
    ctx->errcode = errno;
    return -1;
  }

  return fd;
}

