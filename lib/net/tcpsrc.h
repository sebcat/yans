#ifndef YANS_TCPSRC_H__
#define YANS_TCPSRC_H__

#include <sys/socket.h>
#ifdef __FreeBSD__
#include <drivers/freebsd/tcpsrc/tcpsrc.h>
#endif

struct tcpsrc_ctx {
  /* internal */
  int fd;
};

int tcpsrc_init(struct tcpsrc_ctx *ctx);
int tcpsrc_fdinit(struct tcpsrc_ctx *ctx, int fd);
void tcpsrc_cleanup(struct tcpsrc_ctx *ctx);
int tcpsrc_connect(struct tcpsrc_ctx *ctx, struct sockaddr *sa);

#endif
