#ifndef YANS_SCONN_H__
#define YANS_SCONN_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SCONN_CTX_INITIALIZER {0}

#define sconn_errno(sconn__) (sconn__)->errcode

struct sconn_ctx {
  int errcode; /* saved/fitting errno val, or 0 on success */
};

struct sconn_opts {
  int reuse_addr;            /* true if SO_REUSEADDR should be set */
  int proto;                 /* IPPROTO_TCP, IPPROTO_UDP */
  struct sockaddr *bindaddr; /* address to bind to, if any */
  socklen_t bindaddrlen;     /* length of bindaddr */
  struct sockaddr *dstaddr;  /* address to connect to */
  socklen_t dstaddrlen;      /* length of dstaddr */
};

int sconn_parse_addr(struct sconn_ctx *ctx, const char *addr,
    const char *port, struct sockaddr *dst, socklen_t dstlen);
int sconn_connect(struct sconn_ctx *ctx, struct sconn_opts *opts);

#endif
