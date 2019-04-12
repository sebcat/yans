#include <stdlib.h>

#include <lib/net/tcpproto_types.h>

static const char *names_[] = {
  [TCPPROTO_UNKNOWN] = "unknown",
  [TCPPROTO_SMTP]    = "smtp",
  [TCPPROTO_SMTPS]   = "smtps",
  [TCPPROTO_DNS]     = "dns",
  [TCPPROTO_HTTP]    = "http",
  [TCPPROTO_HTTPS]   = "https",
  [TCPPROTO_POP3]    = "pop3",
  [TCPPROTO_POP3S]   = "pop3s",
  [TCPPROTO_IMAP]    = "imap",
  [TCPPROTO_IMAPS]   = "imaps",
  [TCPPROTO_IRC]     = "irc",
  [TCPPROTO_IRCS]    = "ircs",
  [TCPPROTO_FTP]     = "ftp",
  [TCPPROTO_FTPS]    = "ftps",
  [TCPPROTO_SSH]     = "ssh",
};

/* maps a port number to a tcpproto type */
struct default_tcpproto_entry {
  unsigned short port;
  enum tcpproto_type tcpproto;
};

/* NB: default_tcpprotos_ needs to be ordered by port number or the
 *     call to bsearch(3) later on will produce incorrect results */
static const struct default_tcpproto_entry default_tcpprotos_[] = {
  {.port = 0,     .tcpproto = TCPPROTO_UNKNOWN},
  {.port = 21,    .tcpproto = TCPPROTO_FTP},
  {.port = 22,    .tcpproto = TCPPROTO_SSH},
  {.port = 25,    .tcpproto = TCPPROTO_SMTP},
  {.port = 53,    .tcpproto = TCPPROTO_DNS},
  {.port = 80,    .tcpproto = TCPPROTO_HTTP},
  {.port = 110,   .tcpproto = TCPPROTO_POP3},
  {.port = 143,   .tcpproto = TCPPROTO_IMAP},
  {.port = 443,   .tcpproto = TCPPROTO_HTTPS},
  {.port = 465,   .tcpproto = TCPPROTO_SMTPS},
  {.port = 990,   .tcpproto = TCPPROTO_FTPS},
  {.port = 993,   .tcpproto = TCPPROTO_IMAPS},
  {.port = 995,   .tcpproto = TCPPROTO_POP3S},
  {.port = 6667,  .tcpproto = TCPPROTO_IRC},
  {.port = 6668,  .tcpproto = TCPPROTO_IRC},
  {.port = 6669,  .tcpproto = TCPPROTO_IRC},
  {.port = 6697,  .tcpproto = TCPPROTO_IRCS},
  {.port = 7000,  .tcpproto = TCPPROTO_IRC},
};

const char *tcpproto_type_to_string(enum tcpproto_type t) {
  const char *str;

  if (t < 0 || t >= NBR_OF_TCPPROTOS) {
    t = TCPPROTO_UNKNOWN;
  }

  str = names_[t];
  if (str == NULL) {
    str = "";
  }

  return str;
}

static int port_compar(const void *a, const void *b) {
  const struct default_tcpproto_entry *left = a;
  const struct default_tcpproto_entry *right = b;

  return ((left->port > right->port) - (left->port < right->port));
}

enum tcpproto_type tcpproto_type_from_port(unsigned short port) {
  struct default_tcpproto_entry key = {.port = port};
  struct default_tcpproto_entry *ent;

  ent = bsearch(&key, default_tcpprotos_,
      sizeof(default_tcpprotos_) / sizeof(*default_tcpprotos_),
      sizeof(key), port_compar);
  if (ent == NULL) {
    return TCPPROTO_UNKNOWN;
  }

  return ent->tcpproto;
}

