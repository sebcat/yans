#ifndef TCPPROTO_TYPES_H__
#define TCPPROTO_TYPES_H__

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

const char *tcpproto_type_to_string(enum tcpproto_type t);
enum tcpproto_type tcpproto_type_from_port(unsigned short port);

#endif

