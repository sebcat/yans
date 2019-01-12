#include <string.h>
#include <stdlib.h>

#include <lib/net/tcpproto.h>

/* RE for a tcpproto pattern, as well as the types of the tcpproto if
 * the pattern matches, with either TLS or non-TLS transport */
struct tcpproto_patterns {
  enum tcpproto_type type_plain;
  enum tcpproto_type type_tls;
  const char *pattern;
};

/* tcpproto counter context */
struct pcounter_ctx {
  int counters[NBR_OF_TCPPROTOS];
};

/* maps a port number to a tcpproto type */
struct default_tcpproto_entry {
  unsigned short port;
  enum tcpproto_type tcpproto;
};

static const char *names_[] = {
  [TCPPROTO_UNKNOWN] = "unknown",
  [TCPPROTO_SMTP]    = "smtp",
  [TCPPROTO_SMTPS]   = "smtps",
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

/* NB: default_tcpprotos_ needs to be ordered by port number or the
 *     call to bsearch(3) later on will produce incorrect results */
static const struct default_tcpproto_entry default_tcpprotos_[] = {
  {.port = 0,     .tcpproto = TCPPROTO_UNKNOWN},
  {.port = 21,    .tcpproto = TCPPROTO_FTP},
  {.port = 22,    .tcpproto = TCPPROTO_SSH},
  {.port = 25,    .tcpproto = TCPPROTO_SMTP},
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

static const struct tcpproto_patterns patterns_[] = {
  {
    .type_plain = TCPPROTO_SMTP,
    .type_tls   = TCPPROTO_SMTPS,
    .pattern    = "^[0-9]{3} [^\n]*[Ss][Mm][Tt][Pp]"
  },
  {
    .type_plain = TCPPROTO_FTP,
    .type_tls   = TCPPROTO_FTPS,
    .pattern    = "^[0-9]{3} [^\n]*[Ff][Tt][Pp]"
  },
  {
    .type_plain = TCPPROTO_HTTP,
    .type_tls   = TCPPROTO_HTTPS,
    .pattern    = "^HTTP/1\\.[01] [0-9]+ "
  },
  {
    .type_plain = TCPPROTO_POP3,
    .type_tls   = TCPPROTO_POP3S,
    .pattern    = "^\\+OK [^\n]*[Pp][Oo][Pp]3"
  },
  {
    .type_plain = TCPPROTO_POP3,
    .type_tls   = TCPPROTO_POP3S,
    .pattern    = "^-ERR [^\n]*[Pp][Oo][Pp]3"
  },
  {
    .type_plain = TCPPROTO_IMAP,
    .type_tls   = TCPPROTO_IMAPS,
    .pattern    = "^\\* OK [^\n]*[Ii][Mm][Aa][Pp]"
  },
  {
    .type_plain = TCPPROTO_IRC,
    .type_tls   = TCPPROTO_IRCS,
    .pattern    = "^NOTICE AUTH :"
  },
  {
    .type_plain = TCPPROTO_SSH,
    .type_tls   = TCPPROTO_SSH,
    .pattern    = "^SSH-[0-9]+\\.[0-9]+"
  },
};

static void pcounter_reset(struct pcounter_ctx *c) {
  memset(c, 0, sizeof(struct pcounter_ctx));
}

static void pcounter_count(struct pcounter_ctx *c,
    enum tcpproto_type t) {
  if (t < 0 || t >= NBR_OF_TCPPROTOS) {
    return;
  }

  c->counters[t]++;
  return;
}

static enum tcpproto_type pcounter_max(struct pcounter_ctx *c) {
  int max = 0;
  int i;
  enum tcpproto_type t = TCPPROTO_UNKNOWN;

  for (i = 0; i < NBR_OF_TCPPROTOS; i++) {
    if (c->counters[i] > max) {
      t = i;
    }
  }

  return t;
}

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

int tcpproto_init(struct tcpproto_ctx *ctx) {
  reset_t *r;
  int ret;
  int i;

  r = reset_new();
  if (!r) {
    ctx->err = "failed to instantiate RE set";
    goto fail;
  }

  for (i = 0; i < sizeof(patterns_) / sizeof(*patterns_); i++) {
    ret = reset_add(r, patterns_[i].pattern);
    if (ret < 0) {
      ctx->err = reset_strerror(r);
      goto fail_reset_free;
    }
  }

  ret = reset_compile(r);
  if (ret != RESET_OK) {
    ctx->err = reset_strerror(r);
    goto fail_reset_free;
  }

  ctx->reset = r;
  ctx->err = NULL;
  return 0;
fail_reset_free:
  reset_free(r);
fail:
  return -1;
}

void tcpproto_cleanup(struct tcpproto_ctx *ctx) {
  if (ctx) {
    reset_free(ctx->reset);
    ctx->reset = NULL;
    ctx->err = NULL;
  }
}

enum tcpproto_type tcpproto_match(struct tcpproto_ctx *ctx,
    const char *data, size_t len, int flags) {
  struct pcounter_ctx pcnt;
  int ret;
  int id;
  enum tcpproto_type result = TCPPROTO_UNKNOWN;

  ret = reset_match(ctx->reset, data, len);
  if (ret == RESET_OK) {
    pcounter_reset(&pcnt);
    while ((id = reset_get_next_match(ctx->reset)) >= 0) {
      if (flags & TCPPROTO_MATCHF_TLS) {
        pcounter_count(&pcnt, patterns_[id].type_tls);
      } else {
        pcounter_count(&pcnt, patterns_[id].type_plain);
      }
    }

    result = pcounter_max(&pcnt);
  }

  return result;
}
