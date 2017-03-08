#include <lib/util/netstring.h>

const char *netstring_strerror(int code) {
  switch(code) {
  case NS_OK:
    return "success";
  case NS_ERRFMT:
    return "netstring format error";
  case NS_ERRTOOLARGE:
    return "netstring length too large";
  case NS_ERRINCOMPLETE:
    return "incomplete netstring";
  default:
    return "unknown netstring error";
  }
}

int netstring_parse(char **out, size_t *outlen, char *src, size_t srclen) {
  size_t i;
  size_t len = 0;
  size_t lenbuf;

  enum {
    NS_SLEN,
    NS_SDATA,
  } S = NS_SLEN;

  for (i=0; i < srclen; i++) {
    switch (S) {
    case NS_SLEN:
      if (src[i] >= '0' && src[i] <= '9') {
        lenbuf = len * 10;
        lenbuf += src[i]-'0';
        if (lenbuf < len) {
          /* lenbuf overflow */
          return NS_ERRTOOLARGE;
        }
        len = lenbuf;
        break;
      } else if (i > 0 && src[i] == ':') {
        S = NS_SDATA;
        break;
      } else {
        return NS_ERRFMT;
      }
    case NS_SDATA:
      if (i + len >= srclen) {
        return NS_ERRINCOMPLETE;
      }
      if (src[i+len] != ',') {
        return NS_ERRFMT;
      }
      src[i+len] = '\0';
      *out = src + i;
      *outlen = len;
      return NS_OK;
    }
  }
  return NS_ERRINCOMPLETE;
}
