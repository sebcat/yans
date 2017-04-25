#include <stdio.h>
#include <stdbool.h>

#include <lib/util/netstring.h>

const char *netstring_strerror(int code) {
  switch(code) {
  case NETSTRING_OK:
    return "success";
  case NETSTRING_ERRFMT:
    return "netstring format error";
  case NETSTRING_ERRTOOLARGE:
    return "netstring length too large";
  case NETSTRING_ERRINCOMPLETE:
    return "incomplete netstring";
  case NETSTRING_ERRMEM:
    return "netstring memory allocation error";
  default:
    return "unknown netstring error";
  }
}

int netstring_parse(char **out, size_t *outlen, char *src, size_t srclen) {
  size_t i;
  size_t len = 0;
  size_t lenbuf;

  enum {
    NETSTRING_SLEN,
    NETSTRING_SDATA,
  } S = NETSTRING_SLEN;

  for (i=0; i < srclen; i++) {
    switch (S) {
    case NETSTRING_SLEN:
      if (src[i] >= '0' && src[i] <= '9') {
        lenbuf = len * 10;
        lenbuf += src[i]-'0';
        if (lenbuf < len) {
          /* lenbuf overflow */
          return NETSTRING_ERRTOOLARGE;
        }
        len = lenbuf;
        break;
      } else if (i > 0 && src[i] == ':') {
        S = NETSTRING_SDATA;
        break;
      } else {
        return NETSTRING_ERRFMT;
      }
    case NETSTRING_SDATA:
      if (i + len >= srclen) {
        return NETSTRING_ERRINCOMPLETE;
      }
      if (src[i+len] != ',') {
        return NETSTRING_ERRFMT;
      }
      src[i+len] = '\0';
      *out = src + i;
      if (outlen != NULL) {
        *outlen = len;
      }
      return NETSTRING_OK;
    }
  }
  return NETSTRING_ERRINCOMPLETE;
}

int netstring_append_buf(buf_t *buf, const char *str, size_t len) {
  char szbuf[32];
  int ret;
  size_t oldlen;
  int failcode = NETSTRING_ERRMEM;

  oldlen = buf->len;

  if ((ret = snprintf(szbuf, sizeof(szbuf), "%zu:", len)) <= 1) {
    failcode = NETSTRING_ERRFMT;
    goto fail;
  }

  if (buf_adata(buf, szbuf, (size_t)ret) < 0) {
    goto fail;
  }

  if (buf_adata(buf, str, len) < 0) {
    goto fail;
  }

  if (buf_achar(buf, ',') < 0) {
    goto fail;
  }

  return NETSTRING_OK;

fail:
  buf->len = oldlen;
  return failcode;
}

int netstring_next_pair(struct netstring_pair *res, char **data,
    size_t *datalen) {
  int ret;
  size_t next_off;

  if (*datalen == 0) {
    return NETSTRING_ERRINCOMPLETE;
  }

  ret = netstring_parse(&res->key, &res->keylen, *data, *datalen);
  if (ret != NETSTRING_OK) {
    return ret;
  }

  next_off = (res->key + res->keylen + 1) - *data;
  if (next_off >= *datalen) {
    return NETSTRING_ERRINCOMPLETE;
  }

  ret = netstring_parse(&res->value, &res->valuelen, *data + next_off,
      *datalen - next_off);
  if (ret != NETSTRING_OK) {
    return NETSTRING_ERRINCOMPLETE;
  }

  next_off = (res->value + res->valuelen + 1) - *data;
  *data = *data + next_off;
  *datalen = *datalen - next_off;
  return NETSTRING_OK;
}
