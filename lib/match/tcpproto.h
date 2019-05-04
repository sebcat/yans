#ifndef YANS_TCPPROTO_H__
#define YANS_TCPPROTO_H__

#include <lib/net/tcpproto_types.h>
#include <lib/match/reset.h>

#define TCPPROTO_MATCHF_TLS (1 << 0) /* assume TLS encapsulation */

struct tcpproto_ctx {
  /* internal */
  reset_t *reset;
  const char *err;
};

int tcpproto_init(struct tcpproto_ctx *ctx);
void tcpproto_cleanup(struct tcpproto_ctx *ctx);
enum tcpproto_type tcpproto_match(struct tcpproto_ctx *ctx,
    const char *data, size_t len, int flags);



#endif
