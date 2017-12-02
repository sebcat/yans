#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

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

#define NETSTRING_WS(c) \
    ((c) == '\r' ||     \
     (c) == '\n' ||     \
     (c) == '\t' ||     \
     (c) == ' ')

static int _netstring_parse(const char *src, size_t srclen, size_t *nsoff,
    size_t *nslen) {
  size_t i;
  size_t len = 0;
  size_t lenbuf;

  enum {
    NETSTRING_SWS,
    NETSTRING_SLEN,
    NETSTRING_SDATA,
  } S = NETSTRING_SWS;

  for (i = 0; i < srclen; i++) {
    switch (S) {
    case NETSTRING_SWS:
      if (NETSTRING_WS(src[i])) {
        break;
      }
      S = NETSTRING_SLEN;
      /* fall-through */
    case NETSTRING_SLEN:
      if (src[i] >= '0' && src[i] <= '7') {
        lenbuf = len << 3;
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
      } else if (src[i+len] != ',') {
        return NETSTRING_ERRFMT;
      }

      if (nsoff != NULL) {
        *nsoff = i;
      }

      if (nslen != NULL) {
        *nslen = len;
      }

      return NETSTRING_OK;
    }
  }
  return NETSTRING_ERRINCOMPLETE;
}

int netstring_tryparse(const char *src, size_t srclen, size_t *nextoff) {
  size_t nsoff;
  size_t nslen;
  int ret;

  ret = _netstring_parse(src, srclen, &nsoff, &nslen);
  if (ret == NETSTRING_OK && nextoff != NULL) {
    *nextoff = nsoff + nslen + 1;
  }
  return ret;
}

int netstring_parse(char **out, size_t *outlen, char *src, size_t srclen) {
  size_t nsoff;
  size_t nslen;
  int ret;

  ret = _netstring_parse(src, srclen, &nsoff, &nslen);
  if (ret == NETSTRING_OK) {
    src[nsoff + nslen] = '\0';
    *out = src + nsoff;
    if (outlen != NULL) {
      *outlen = nslen;
    }
  }
  return ret;
}

int netstring_append_buf(buf_t *buf, const char *str, size_t len) {
  char szbuf[48];
  char *szptr;
  size_t oldlen;
  size_t tmplen;
  int failcode = NETSTRING_ERRMEM;

  oldlen = buf->len;

  tmplen = len;
  szptr = szbuf + sizeof(szbuf) - 1;
  *szptr = ':';
  do {
    szptr--;
    *szptr = '0' + (tmplen & 0x07);
    tmplen = tmplen >> 3;
  } while (tmplen != 0);

  if (buf_adata(buf, szptr, szbuf + sizeof(szbuf) - szptr) < 0) {
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

int netstring_append_list(buf_t *buf, size_t nelems, const char **elems,
    size_t *lens) {
  size_t i;
  size_t oldlen;
  int ret;

  oldlen = buf->len;
  for (i = 0; i < nelems; i++) {
    ret = netstring_append_buf(buf, elems[i], lens[i]);
    if (ret != NETSTRING_OK) {
      goto fail;
    }
  }

  return NETSTRING_OK;
fail:
  buf->len = oldlen;
  return ret;
}

int netstring_append_pair(buf_t *buf, const char *str0, size_t len0,
    const char *str1, size_t len1) {
  size_t oldlen;
  int ret;

  oldlen = buf->len;
  ret = netstring_append_buf(buf, str0, len0);
  if (ret != NETSTRING_OK) {
    goto fail;
  }

  ret = netstring_append_buf(buf, str1, len1);
  if (ret != NETSTRING_OK) {
    goto fail;
  }

  return NETSTRING_OK;

fail:
  buf->len = oldlen;
  return ret;
}

int netstring_next(char **out, size_t *outlen, char **data, size_t *datalen) {
  int ret;
  size_t next_off;

  if (*datalen == 0) {
    return NETSTRING_ERRINCOMPLETE;
  }

  ret = netstring_parse(out, outlen, *data, *datalen);
  if (ret != NETSTRING_OK) {
    return ret;
  }

  next_off = (*out + *outlen + 1) - *data;
  if (next_off > *datalen) {
    return NETSTRING_ERRINCOMPLETE;
  }

  *data = *data + next_off;
  *datalen = *datalen - next_off;
  return NETSTRING_OK;
}

int netstring_next_pair(struct netstring_pair *res, char **data,
    size_t *datalen) {
  int ret;
  size_t next_off;

  /* There's some similarities with netstring_next here, the difference being
   * that data and datalen is only updated on a successfully read pair, while
   * netstring_next updates data and datalen for every read string */

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

int netstring_serialize(void *data, struct netstring_map *map, buf_t *out) {
  char **value;
  size_t *valuelen;
  int ret = NETSTRING_OK;
  buf_t tmp; /* it would be nice to avoid this temporary allocation */

  buf_init(&tmp, 1024);
  while (map->key != NULL) {
    value = (char**)((char*)data + map->voff);
    /* serialize empty strings to NULL for non-ambiguity */
    if (*value != NULL && **value != '\0') {
      valuelen = (size_t*)((char*)data + map->loff);
      ret = netstring_append_pair(&tmp, map->key, strlen(map->key),
          *value, *valuelen);
      if (ret != NETSTRING_OK) {
        goto done;
      }
    }

    map++;
  }

  ret = netstring_append_buf(out, tmp.data, tmp.len);
done:
  buf_cleanup(&tmp);
  return ret;
}

int netstring_deserialize(void *data, struct netstring_map *map, char *in,
    size_t inlen, size_t *left) {
  int ret = NETSTRING_OK;
  char *curr;
  char **outval;
  size_t *outlen;
  size_t len;
  struct netstring_pair pair;
  struct netstring_map *mcurr, *mnext;

  if (map->key == NULL) {
    goto done;
  }

  ret = netstring_parse(&curr, &len, in, inlen);
  if (ret != NETSTRING_OK) {
    goto done;
  }

  if (left != NULL) {
    *left = inlen - ((curr + len + 1) - in);
  }

  mcurr = map;
  while (netstring_next_pair(&pair, &curr, &len) == NETSTRING_OK) {
    mnext = mcurr;
    do {
      if (strcmp(mnext->key, pair.key) == 0) {
        if (pair.value && *pair.value != '\0') {
          outval = (char**)((char*)data + mnext->voff);
          outlen = (size_t*)((char*)data + mnext->loff);
          *outval = pair.value;
          *outlen = pair.valuelen;
        }
        mcurr = mnext;
        break;
      }

      mnext++;
      if (mnext->key == NULL) {
        mnext = map;
      }
    } while (mnext != mcurr);
  }

done:
  return ret;
}
