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

int netstring_serialize(void *data, struct netstring_map *map, buf_t *out) {
  char **value;
  int ret = NETSTRING_OK;
  buf_t tmp; /* it would be nice to avoid this temporary allocation */

  buf_init(&tmp, 1024);
  while (map->key != NULL) {
    /* XXX: the offset *must* be divisable by the size of a char pointer.
     *      for this to happen, the struct fields must be properly aligned.
     *      Ideally the struct should only consist of char pointer fields */
    assert((map->offset & (sizeof(char*)-1)) == 0);
    value = data;
    value += map->offset / sizeof(char*);
    /* serialize empty strings to NULL for non-ambiguity */
    if (*value != NULL && **value != '\0') {
      ret = netstring_append_pair(&tmp, map->key, strlen(map->key),
          *value, strlen(*value));
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
    size_t inlen) {
  int ret = NETSTRING_OK;
  char *curr;
  char **value;
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

  mcurr = map;
  while (netstring_next_pair(&pair, &curr, &len) == NETSTRING_OK) {
    mnext = mcurr;
    do {
      if (strcmp(mnext->key, pair.key) == 0) {
        if (pair.value && *pair.value != '\0') {
          assert((mnext->offset & (sizeof(char*)-1)) == 0);
          value = data;
          value += mnext->offset / sizeof(char*);
          *value = pair.value;
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
