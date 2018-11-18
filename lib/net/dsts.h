#ifndef YANS_DSTS_H__
#define YANS_DSTS_H__

#include <lib/util/buf.h>
#include <lib/net/ip.h>
#include <lib/net/ports.h>

struct dst_ctx {
  /* internal */
  struct ip_blocks addrs;
  struct port_ranges ports;
  void *data;
};

struct dsts_ctx {
  /* internal */
  int flags;
  buf_t buf;                  /* array of struct dst */
  size_t dsts_next;           /* index into 'dsts' for the next dst */
  struct dst_ctx currdst;     /* current dst from dsts */
  ip_addr_t curraddr;         /* current IP address from currdst */
};

int dsts_init(struct dsts_ctx *dsts);
void dsts_cleanup(struct dsts_ctx *dsts);

int dsts_add(struct dsts_ctx *dsts, const char *addrs, const char *ports,
    void *dstdata);
int dsts_next(struct dsts_ctx *dsts, struct sockaddr *dst,
    socklen_t *dstlen, void **data);


#endif
