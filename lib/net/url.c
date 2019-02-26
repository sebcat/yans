/**
 * url.c - Routines for normalizing URLs and and resolving relative
 * references based on RFC3986, though some parts do not adhere to
 * the RFC (empty queries have their ? removed, empty fragments have their #
 * removed), and some parts go beyond (e.g., normalization of
 * host names (IDNA) and paths (URL-encoding)).
 *
 * Permits only http and https schemes. Also works with empty schemes
 * where HTTP/HTTPS semantics are expected (e.g., "//example.com/").
 *
 * Getting rid of the regular expression in favor of a non-allocing
 * solution would be nice - valgrind/massif has it as the biggest
 * source of allocations in this file.
 */
#include <sys/types.h>
#include <regex.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include <lib/util/buf.h>
#include <lib/net/punycode.h>
#include <lib/net/ip.h>
#include <lib/net/url.h>

#define URL_INITALLOC  512

#define URL_MAXHOSTLEN 256 /* RFC1035 */

struct url_ctx_t {
  struct url_opts opts;
  struct punycode_ctx punycode;
  regex_t re_parse;
};

/* scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) */
#define URLSCHEME "[a-zA-Z][A-Za-z0-9+.-]*"
#define URLRE \
    "^(("URLSCHEME"):)?(//([^/?#]*))?([^?#]*)(\\?([^#]*))?(#(.*))?"
/*    12               3  4          5       6   7        8 9 */
#define URLRE_NGROUPS         10 /* including complete match */
#define URLRE_SCHEMEI          2
#define URLRE_AUTHI            4
#define URLRE_PATHI            5
#define URLRE_FULL_QUERYI      6
#define URLRE_QUERYI           7
#define URLRE_FULL_FRAGMENTI   8
#define URLRE_FRAGMENTI        9

url_ctx_t *url_ctx_new(struct url_opts *opts) {
  url_ctx_t *ctx;
  int ret;

  if ((ctx = calloc(1, sizeof(url_ctx_t))) == NULL) {
    goto fail;
  }

  ret = regcomp(&ctx->re_parse, URLRE, REG_NEWLINE|REG_EXTENDED);
  if (ret != 0) {
    goto fail_free;
  }

  ret = punycode_init(&ctx->punycode);
  if (ret < 0) {
    goto fail_regfree;
  }

  if (opts != NULL) {
    memcpy(&ctx->opts, opts, sizeof(struct url_opts));
  }

  return ctx;
fail_regfree:
  regfree(&ctx->re_parse);
fail_free:
  free(ctx);
fail:
  return NULL;
}

void url_ctx_free(url_ctx_t *ctx) {
  if (ctx != NULL) {
    punycode_cleanup(&ctx->punycode);
    regfree(&ctx->re_parse);
    free(ctx);
  }
}

const struct url_opts *url_ctx_opts(url_ctx_t *ctx) {
  return &ctx->opts;
}

/* authority   = [ userinfo "@" ] host [ ":" port ]
 * userinfo    = *( unreserved / pct-encoded / sub-delims / ":" )
 * host        = IP-literal / IPv4address / reg-name
 * port        = *DIGIT
 * reg-name    = *( unreserved / pct-encoded / sub-delims )
 * IP-literal  = "[" ( IPv6address / IPvFuture  ) "]"
 * IPvFuture   = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" )
 */
static int url_parse_auth(const char *s, struct url_parts *out) {
  const char *pos, *cptr;
  size_t len;

  if (out->authlen == 0) {
    return 0;
  }

  pos = s + out->auth;
  len = out->authlen;
  if ((cptr = memchr(pos, '@', len)) != NULL) {
    out->flags |= URLPART_HAS_USERINFO;
    out->userinfolen = cptr - pos;
    out->userinfo = pos - s;
    pos += out->userinfolen + 1;
    len -= out->userinfolen + 1;
  }

  if (len == 0) {
    return 0;
  }

  if (*pos == '[') {
    /* IP-literal */
    if ((cptr = memchr(pos, ']', len)) != NULL) {
      out->hostlen = cptr - pos + 1;
      out->host = pos - s;
      pos += out->hostlen;
      len -= out->hostlen;
      if (len > 0 && *pos == ':') {
        out->flags |= URLPART_HAS_PORT;
        out->portlen = len - 1;
        out->port = (pos-s) + 1;
      }
    } else {
      return -1;
    }
  } else {
    /* non IP-literal */
    if ((cptr = memchr(pos, ':', len)) != NULL) {
      /* have port */
      out->flags |= URLPART_HAS_PORT;
      out->hostlen = cptr - pos;
      out->host = pos-s;
      pos += out->hostlen + 1;
      len -= out->hostlen + 1;
      if (len > 0) {
        out->portlen = len;
        out->port = (pos-s);
      }
    } else {
      /* no port */
      out->hostlen = len;
      out->host = pos-s;
    }
  }

  return 0;
}

int url_parse(url_ctx_t *ctx, const char *s, struct url_parts *out) {
  regmatch_t m[URLRE_NGROUPS] = {{0}};

  memset(out, 0, sizeof(*out));
  if (s == NULL || *s == '\0') {
    return 0;
  }

  if (regexec(&ctx->re_parse, s, URLRE_NGROUPS, m, 0) != 0) {
    return -1;
  }

  if (m[URLRE_SCHEMEI].rm_so >= 0) {
    out->scheme = (size_t)m[URLRE_SCHEMEI].rm_so;
    out->schemelen = (size_t)
        (m[URLRE_SCHEMEI].rm_eo - m[URLRE_SCHEMEI].rm_so);
  }

  if (m[URLRE_AUTHI].rm_so >= 0) {
    out->auth = (size_t)m[URLRE_AUTHI].rm_so;
    out->authlen = (size_t)
        (m[URLRE_AUTHI].rm_eo - m[URLRE_AUTHI].rm_so);

  }

  if (m[URLRE_PATHI].rm_so >= 0) {
    out->path = (size_t)m[URLRE_PATHI].rm_so;
    out->pathlen = (size_t)
        (m[URLRE_PATHI].rm_eo - m[URLRE_PATHI].rm_so);
    if (url_parse_auth(s, out) < 0) {
      return -1;
    }
  }

  if (m[URLRE_FULL_QUERYI].rm_so >= 0) {
    out->flags |= URLPART_HAS_QUERY;
  }

  if (m[URLRE_QUERYI].rm_so >= 0) {
    out->query = (size_t)m[URLRE_QUERYI].rm_so;
    out->querylen = (size_t)
        (m[URLRE_QUERYI].rm_eo - m[URLRE_QUERYI].rm_so);

  }

  if (m[URLRE_FULL_FRAGMENTI].rm_so >= 0) {
    out->flags |= URLPART_HAS_FRAGMENT;
  }

  if (m[URLRE_FRAGMENTI].rm_so >= 0) {
    out->fragment = (size_t)m[URLRE_FRAGMENTI].rm_so;
    out->fragmentlen = (size_t)
        (m[URLRE_FRAGMENTI].rm_eo - m[URLRE_FRAGMENTI].rm_so);
  }

  return 0;
}

size_t url_remove_dot_segments(char *s, size_t len) {
  size_t r=0, w=0;
  /*
   *   this is written to make is somewhat comparable to the wording of the
   *   RFC, and not for performance.
   *
   *   It is interesting to compare this with  "Lexical File Names in Plan 9"
   *   by Rob Pike. The UNIX way was better even before the URL RFC was
   *   written. e.g., /foo//bar != /foo/bar in RFC3986, trailing slashes are
   *   significant, &c */
  while (r < len) {
    if (r+1 < len && s[r] == '.' && s[r+1] == '/') {
      r+=2;
    } else if (r+2 < len && s[r] == '.' && s[r+1] == '.' &&
        s[r+2] == '/') {
      r+=3;
    } else if (r+2 < len && s[r] == '/' && s[r+1] == '.' &&
        s[r+2] == '/') {
      r+=2;
    } else if (r+2 == len && s[r] == '/' && s[r+1] == '.') {
      s[++r] = '/';
    } else if (r+3 < len && s[r] == '/' && s[r+1] == '.' &&
        s[r+2] == '.' && s[r+3] == '/') {
      r+=3;
      while(w > 0 && s[--w] != '/');
    } else if (r+3 == len && s[r] == '/' && s[r+1] == '.' &&
        s[r+2] == '.') {
      r+=2;
      s[r] = '/';
      while(w > 0 && s[--w] != '/');
    } else if (r+1 == len && s[r] == '.') {
      r++;
    } else if (r+2 == len && s[r] == '.' && s[r+1] == '.') {
      r+=2;
    } else {
      do {
        s[w++] = s[r++];
      } while (r < len && s[r] != '/');
    }
  }

  return w;
}

#define ISPCHAR(x) \
  (((x) >= 'A' && (x) <= 'Z') || \
   ((x) >= 'a' && (x) <= 'z') || \
   ((x) >= '0' && (x) <= '9') || \
   (strchr("-._~!$&'()*+,;=:@", (x)) != NULL))

#define ISQUERYCHAR(x) \
  (((x) >= 'A' && (x) <= 'Z') || \
   ((x) >= 'a' && (x) <= 'z') || \
   ((x) >= '0' && (x) <= '9') || \
   (strchr("-._~!$&'()*+,;=:@/?", (x)) != NULL))

int url_supported_scheme(const char *s) {
  if (strcmp(s, "http") != 0 &&
      strcmp(s, "https") != 0) {
    return EURL_SCHEME;
  }
  return EURL_OK;
}

static int normalize_host(struct punycode_ctx *punycode, const char *host,
    size_t hostlen, char *dst, size_t dstlen) {
  char tmp[URL_MAXHOSTLEN];
  char *oname;
  ip_addr_t addr;
  int i;

  if (hostlen >= URL_MAXHOSTLEN || hostlen >= dstlen) {
    return EURL_HOST;
  }

  snprintf(tmp, sizeof(tmp), "%.*s", (int)hostlen, host);

  /* is it an address? */
  if (ip_addr(&addr, tmp, NULL) == 0) {
    if (ip_addr_str(&addr, dst, dstlen, NULL) != 0) {
      return EURL_HOST;
    }
    return EURL_OK;
  }

  /* do we need to punycode encode it? */
  for (i = 0; i < hostlen; i++) {
    if ((unsigned char)tmp[i] < 0x1f) {
      return EURL_HOST;
    } else if ((unsigned char)tmp[i] > 0x7f) {
      break;
    }
  }
  if (i == hostlen) {
    /* No need to punycode encode the host, just copy it */
    snprintf(dst, dstlen, "%s", tmp);
    return EURL_OK;
  }

  if ((oname = punycode_encode(punycode, host, hostlen)) == NULL) {
    return EURL_HOST;
  }

  snprintf(dst, dstlen, "%s", oname);
  return EURL_OK;
}

int url_normalize(url_ctx_t *ctx, const char *s, buf_t *out) {
  struct url_parts parts;
  size_t i;
  char hexbuf[4];
  char hostbuf[URL_MAXHOSTLEN];
  const char *hextbl = "0123456789abcdef";
  int ret;

  hexbuf[0] = '%';
  buf_clear(out);
  if (url_parse(ctx, s, &parts) < 0) {
    return EURL_PARSE;
  }

  if (parts.schemelen > 0) {
    for(i=parts.scheme; i<parts.schemelen; i++) {
      if (s[i] < 0x20 || s[i] > 0x7f) {
        return EURL_SCHEME;
      }
      buf_achar(out, tolower(s[i]));
    }
    buf_achar(out, 0);
    if (url_supported_scheme(out->data) != EURL_OK) {
      return EURL_SCHEME;
    }
    buf_shrink(out, 1);
    buf_achar(out, ':');
  }

  if (parts.authlen > 0) {
    buf_adata(out, "//", 2);
  }

  if (parts.userinfolen > 0) {
    buf_adata(out, s+parts.userinfo, parts.userinfolen);
    buf_achar(out, '@');
  }

  if (parts.hostlen > 255) { /* RFC1035 */
    return EURL_HOST;
  } else if (parts.hostlen > 0) {
    ret = normalize_host(&ctx->punycode, s+parts.host, parts.hostlen,
        hostbuf, sizeof(hostbuf));
    if (ret != EURL_OK) {
      return ret;
    }
    buf_adata(out, hostbuf, strlen(hostbuf));
  }

  if (parts.portlen > 0) {
    buf_achar(out, ':');
    buf_adata(out, s+parts.port, parts.portlen);
  }

  if (parts.authlen > 0 && parts.pathlen == 0) {
    buf_achar(out, '/');
  } else if (parts.pathlen > 0) {
    size_t eoff = parts.path+parts.pathlen;
    size_t pstart = out->len, nreduced;
    size_t npathlen;
    for(i=parts.path; i<eoff; i++) {
      if (ISPCHAR(s[i]) || s[i] == '/') {
        buf_achar(out, s[i]);
      } else if (s[i] == '%' && i+2 < eoff && isxdigit(s[i+1]) &&
          isxdigit(s[i+2])) {
        buf_achar(out, s[i]);
      } else {
        hexbuf[1] = hextbl[(s[i]&0xff)>>4];
        hexbuf[2] = hextbl[s[i]&0x0f];
        buf_adata(out, hexbuf, 3);
      }
    }

    npathlen = url_remove_dot_segments(out->data + pstart,
        out->len - pstart);
    nreduced = out->len - (pstart + npathlen);
    buf_shrink(out, nreduced);
    if (parts.authlen > 0 && nreduced == parts.pathlen) {
      buf_achar(out, '/');
    }
  }

  if (parts.querylen > 0) {
    size_t eoff = parts.query+parts.querylen;
    buf_achar(out, '?');
    for(i=parts.query; i<eoff; i++) {
      if (ISQUERYCHAR(s[i])) {
        buf_achar(out, s[i]);
      } else if (s[i] == '%' && i+2 < eoff && isxdigit(s[i+1]) &&
          isxdigit(s[i+2])) {
        buf_achar(out, s[i]);
      } else {
        hexbuf[1] = hextbl[(s[i]&0xff)>>4];
        hexbuf[2] = hextbl[s[i]&0x0f];
        buf_adata(out, hexbuf, 3);
      }
    }
  } else if (parts.flags & URLPART_HAS_QUERY &&
      !(ctx->opts.flags & URLFL_REMOVE_EMPTY_QUERY)) {
    buf_achar(out, '?');
  }

  if (parts.fragmentlen > 0) {
    buf_achar(out, '#');
    buf_adata(out, s+parts.fragment, parts.fragmentlen);
  } else if (parts.flags & URLPART_HAS_FRAGMENT &&
      !(ctx->opts.flags & URLFL_REMOVE_EMPTY_FRAGMENT)) {
    buf_achar(out, '#');
  }

  buf_achar(out, '\0');
  return EURL_OK;
}

int url_resolve(url_ctx_t *ctx, const char *base, const char *inref,
    buf_t *out) {
  struct url_parts baseparts, refparts;
  int ret = EURL_PARSE;
  buf_t ref;

  buf_clear(out);
  if ((buf_init(&ref, URL_INITALLOC)) == NULL) {
    return EURL_MEM;
  }

  if ((ret = url_normalize(ctx, inref, &ref)) != EURL_OK) {
    goto fail;
  }

  if (url_parse(ctx, base, &baseparts) < 0) {
    goto fail;
  }

  if (url_parse(ctx, ref.data, &refparts) < 0) {
    goto fail;
  }

  /* reference URL has a scheme */
  if (refparts.schemelen > 0) {
    buf_adata(out, ref.data, ref.len-1);
    goto done;
  }

  if (baseparts.schemelen > 0) {
    buf_adata(out, base + baseparts.scheme, baseparts.schemelen);
    buf_achar(out, ':');
  }

  /* reference URL does not have a scheme but has an authority */
  if (refparts.authlen > 0) {
    buf_adata(out, "//", 2);
    buf_adata(out, ref.data + refparts.auth , ref.len - refparts.auth - 1);
    goto done;
  }

  /* reference URL does not have a scheme or an authority */
  if (baseparts.authlen > 0) {
    buf_adata(out, "//", 2);
    buf_adata(out, base + baseparts.auth, baseparts.authlen);
  }

  if (refparts.pathlen == 0) { /* empty path */
    if (baseparts.pathlen > 0) {
      buf_adata(out, base + baseparts.path, baseparts.pathlen);
    }

    if (refparts.querylen > 0) { /* empty path, non-empty query */
      buf_achar(out, '?');
      buf_adata(out, ref.data + refparts.query, refparts.querylen);
    } else if (baseparts.querylen > 0) { /* empty path, !empty base q */
      buf_achar(out, '?');
      buf_adata(out, base + baseparts.query, baseparts.querylen);
    }
  } else if (ref.data[refparts.path] == '/') { /* absolute path */
    buf_adata(out, ref.data + refparts.path, refparts.pathlen);
    if (refparts.querylen > 0) { /* absolute path, with query */
      buf_achar(out, '?');
      buf_adata(out, ref.data + refparts.query, refparts.querylen);
    }
  } else { /* relative path */
    /* merge path */
    if (baseparts.pathlen == 0) {
      buf_achar(out, '/');
      buf_adata(out, ref.data + refparts.path, refparts.pathlen);
    } else {
      ssize_t i, pos;
      for(i=baseparts.pathlen-1, pos=-1; i >= 0; i--) {
        if (base[baseparts.path+i] == '/') {
          pos = i;
          break;
        }
      }
      if (pos >= 0) {
        buf_adata(out, base + baseparts.path, pos+1);
      }
      buf_adata(out, ref.data + refparts.path, refparts.pathlen);
    }

    if (refparts.querylen > 0) { /* relative path, with query */
      buf_achar(out, '?');
      buf_adata(out, ref.data + refparts.query, refparts.querylen);
    }
  }

  if (refparts.fragmentlen > 0) {
    buf_achar(out, '#');
    buf_adata(out, ref.data + refparts.fragment, refparts.fragmentlen);
  }

done:
  buf_achar(out, '\0');
  buf_cleanup(&ref);
  return EURL_OK;
fail:
  buf_cleanup(&ref);
  return ret;
}

const char *url_errstr(int code) {
  switch(code) {
    case EURL_OK:
      return "url: OK";
    case EURL_MEM:
      return "url: out of memory";
    case EURL_PARSE:
      return "url: unable to parse URL";
    case EURL_SCHEME:
      return "url: unsupported scheme";
    case EURL_HOST:
      return "url: invalid host";
    default:
      return "url: unknown error";
  }
}


