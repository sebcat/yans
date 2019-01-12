#ifndef YANS_TCPPROTO_H__
#define YANS_TCPPROTO_H__

#include <lib/util/reset.h>

#define TCPPROTO_MATCHF_TLS (1 << 0) /* assume TLS encapsulation */

/* When adding tcpprotos to this enum: add them at the end, right
 * before NBR_OF_TCPPROTOS. This keeps the values consistent. */
enum tcpproto_type {
  TCPPROTO_UNKNOWN = 0,
  TCPPROTO_SMTP,
  TCPPROTO_SMTPS,
  TCPPROTO_HTTP,
  TCPPROTO_HTTPS,
  TCPPROTO_POP3,
  TCPPROTO_POP3S,
  TCPPROTO_IMAP,
  TCPPROTO_IMAPS,
  TCPPROTO_IRC,
  TCPPROTO_IRCS,
  TCPPROTO_FTP,
  TCPPROTO_FTPS,
  TCPPROTO_SSH,
  NBR_OF_TCPPROTOS,
};

struct tcpproto_ctx {
  /* internal */
  reset_t *reset;
  const char *err;
};

const char *tcpproto_type_to_string(enum tcpproto_type t);
enum tcpproto_type tcpproto_type_from_port(unsigned short port);

int tcpproto_init(struct tcpproto_ctx *ctx);
void tcpproto_cleanup(struct tcpproto_ctx *ctx);
enum tcpproto_type tcpproto_match(struct tcpproto_ctx *ctx,
    const char *data, size_t len, int flags);



#endif
