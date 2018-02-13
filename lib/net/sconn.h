#ifndef YANS_SCONN_H__
#define YANS_SCONN_H__

#define SCONN_CTX_INITIALIZER {0}

#define sconn_errno(sconn__) (sconn__)->errcode

struct sconn_ctx {
  int errcode; /* saved/fitting errno val, or 0 on success */
};

struct sconn_opts {
  int nretries;         /* number of retries, <= 0 means no retries */
  int reuse_addr;       /* true if SO_REUSEADDR should be set */
  const char *proto;    /* "tcp", or "udp" */
  const char *bindaddr; /* local address to bind to, or NULL */
  const char *bindport; /* local port to bind to, or NULL */
  const char *dstaddr;  /* numerical destination address */
  const char *dstport;  /* numerical destination port */
};

int sconn_connect(struct sconn_ctx *ctx, struct sconn_opts *opts);

#endif
