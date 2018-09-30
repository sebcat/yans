#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lib/net/sconn.h>

static int get_protocol(struct sconn_ctx *ctx, struct sconn_opts *opts) {

  if (opts->proto == NULL) {
    goto fail;
  } else if (strcmp(opts->proto, "tcp") == 0) {
    return IPPROTO_TCP;
  } else if (strcmp(opts->proto, "udp") == 0) {
    return IPPROTO_UDP;
  }
fail:
  ctx->errcode = EINVAL;
  return -1;
}

/* map EAI to suitable errno. A bit hacky. */
static int eai2errno(int eai) {
  switch(eai) {
  case EAI_SYSTEM:
    return errno;
  case EAI_MEMORY:
    return ENOMEM;
  case EAI_AGAIN:
    return EAGAIN; /* we may be able to handle these */
  case EAI_NONAME:
    return EADDRNOTAVAIL;
  default:
    return EINVAL;
  }
}

static int bindaddr(int fd, const char *bindaddr, const char *bindport,
    int family, int socktype, int protocol, int *outerr) {
  struct addrinfo srchints;
  struct addrinfo *srcaddrs = NULL;
  struct addrinfo *currsrc;
  int ret;

  srchints.ai_flags = AI_ADDRCONFIG | AI_NUMERICHOST | AI_NUMERICSERV;
  srchints.ai_family = family;
  srchints.ai_socktype = socktype;
  srchints.ai_protocol = protocol;
  ret = getaddrinfo(bindaddr, bindport, &srchints, &srcaddrs);
  if (ret != 0) {
    *outerr = eai2errno(ret);
    goto err;
  }

  for (currsrc = srcaddrs; currsrc != NULL; currsrc = currsrc->ai_next) {
    ret = bind(fd, currsrc->ai_addr, currsrc->ai_addrlen);
    if (ret == 0) {
      freeaddrinfo(srcaddrs);
      return 0;
    }
  }

  freeaddrinfo(srcaddrs);
err:
  return -1;
}

static int _sconn_connect(struct sconn_ctx *ctx, struct sconn_opts *opts) {
  struct addrinfo dsthints = {0};
  struct addrinfo *dstaddrs = NULL;
  struct addrinfo *currdst;
  int protocol;
  int socktype;
  int family;
  int fd;
  int ret;
  int stored_err = 0;
  int reuse = 1;

  /* get the protocol and socket type */
  protocol = get_protocol(ctx, opts);
  if (protocol < 0) {
    return -1;
  }
  socktype = protocol == IPPROTO_TCP ? SOCK_STREAM : SOCK_DGRAM;

  /* setup hints and call getaddrinfo.
   * Only return addresses if we have a source address for that address family
   * and use numeric host and service names because we don't want name lookups
   * to happen, even for services. */
  dsthints.ai_flags = AI_ADDRCONFIG | AI_NUMERICHOST | AI_NUMERICSERV;
  dsthints.ai_family = AF_UNSPEC;
  dsthints.ai_protocol = protocol;
  dsthints.ai_socktype = socktype;
  ret = getaddrinfo(opts->dstaddr, opts->dstport, &dsthints, &dstaddrs);
  if (ret != 0) {
    ctx->errcode = eai2errno(ret);
    return -1;
  }

  for (currdst = dstaddrs; currdst != NULL; currdst = currdst->ai_next) {
    family = currdst->ai_family;
    if (family != AF_INET && family != AF_INET6) {
      continue;
    }

    fd = socket(family, socktype | SOCK_NONBLOCK, protocol);
    if (fd < 0) {
      stored_err = errno;
      continue;
    }

    if (opts->reuse_addr) {
      setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    }

    if (opts->bindaddr != NULL || opts->bindport != NULL) {
      ret = bindaddr(fd, opts->bindaddr, opts->bindport,
          family, socktype, protocol, &stored_err);
      if (ret < 0) {
        close(fd);
        stored_err = errno;
        continue;
      }
    }

    ret = connect(fd, currdst->ai_addr, currdst->ai_addrlen);
    if (ret >= 0 || errno == EINPROGRESS || errno == EINTR) {
      /* successful or ongoing connection attempt - return fd.
       * EINTR because man says:
       *   "The connection will be established in the background, as in the
       *    case of EINPROGRESS"
       */
      freeaddrinfo(dstaddrs);
      return fd;
    }

    /* connection failed, retry the rest of the addresses but store the err */
    close(fd);
    stored_err = errno;
  }

  freeaddrinfo(dstaddrs);
  if (stored_err != 0) {
    ctx->errcode = stored_err;
  } else {
    ctx->errcode = EINVAL;
  }
  return -1;
}

/* returns socket in non-blocking mode or -1 with set errcode in ctx */
int sconn_connect(struct sconn_ctx *ctx, struct sconn_opts *opts) {
  int nretries;
  int ret;

  /* set initial state */
  ctx->errcode = 0;
  nretries = opts->nretries > 0 ? opts->nretries : 0;

  /* retry loop. We may want to do this only for the call to connect */
  do {
    ret = _sconn_connect(ctx, opts);
  } while (ret < 0 && --nretries > 0);

  return ret;
}

