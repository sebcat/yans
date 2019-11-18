/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#include <string.h>
#include <stdlib.h>

#include <lib/match/tcpproto.h>

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

static const struct tcpproto_patterns patterns_[] = {
  {
    .type_plain = TCPPROTO_SMTP,
    .type_tls   = TCPPROTO_SMTPS,
    .pattern    = "^[0-9]{3}[ -][^\n]*[Ss][Mm][Tt][Pp]"
  },
  {
    .type_plain = TCPPROTO_FTP,
    .type_tls   = TCPPROTO_FTPS,
    .pattern    = "^[0-9]{3}[ -][^\n]*[Ff][Tt][Pp]"
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
    ret = reset_add_pattern(r, patterns_[i].pattern);
    if (ret == RESET_ERR) {
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
