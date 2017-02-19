/* RFC3492 punycode */
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "u8.h"
#include "buf.h"

#define ACE_PREFIX     "xn--"
#define ACE_PREFIX_LEN (sizeof(ACE_PREFIX)-1)

/* RFC3492 Section 5, bootstring values */
#define BS_BASE           36
#define BS_TMIN            1
#define BS_TMAX           26
#define BS_SKEW           38
#define BS_DAMP          700
#define BS_INITIAL_BIAS   72
#define BS_INITIAL_N     128

/* RFC3492 6.1, bias adaption function */
static int32_t adapt(int32_t delta, int32_t npoints, int first_time) {
  int32_t k = 0;
  delta = delta / (first_time ? BS_DAMP : 2);
  delta += delta / npoints;
  while(delta > ((BS_BASE-BS_TMIN)*BS_TMAX) / 2) {
    delta = delta / (BS_BASE - BS_TMIN);
    k += BS_BASE;
  }
  return k + (BS_BASE - BS_TMIN + 1)*delta/(delta+BS_SKEW);
}

static int32_t enc_digit(int32_t digit) {
  if (digit >= 0 && digit < 26) {
    return digit+'a';
  } else if (digit >= 26 && digit < 36) {
    return digit + ('0' - 26);
  }
  assert(0);
  return 0;
}

/* RFC3492 6.3, Encoding procedure */
static int encode_label(buf_t *buf, const uint8_t *s, size_t len) {
  int32_t delta = 0, n = BS_INITIAL_N, bias = BS_INITIAL_BIAS;
  int32_t h, b = 0, remaining = 0;
  size_t pos, width;

  assert(buf != NULL);
  assert(s != NULL);
  assert(len > 0);
  buf_adata(buf, ACE_PREFIX, ACE_PREFIX_LEN);
  for(pos = 0; pos < len; pos += width) {
    int32_t r = u8_to_cp(s+pos, len-pos, &width);
    if (r <= 0x7f) {
      b++;
      buf_achar(buf, (int)r);
    } else {
      remaining++;
    }
  }
  h = b;
  if (b > 0) {
    buf_achar(buf, '-');
  }
  while(remaining) {
    int32_t m = 0x7fffffff;
    for(pos = 0; pos < len; pos += width) {
      int32_t r = u8_to_cp(s+pos, len-pos, &width);
      if (m > r && r >= n) {
        m = r;
      }
    }
    delta += (m - n) * (h + 1);
    if (delta < 0) {
      return -1;
    }
    n = m;
    for(pos = 0; pos < len; pos += width) {
      int32_t r = u8_to_cp(s+pos, len-pos, &width);
      if (r < n) {
        delta++;
        if (delta < 0) {
          return -1;
        }
        continue;
      }
      if (r > n) {
        continue;
      }
      int32_t q = delta;
      int32_t k;
      for(k = BS_BASE; ;k += BS_BASE) {
        int32_t t = k - bias;
        if (t < BS_TMIN) {
          t = BS_TMIN;
        } else if (t > BS_TMAX) {
          t = BS_TMAX;
        }
        if (q < t) {
          break;
        }
        buf_achar(buf, enc_digit(t+(q-t)%(BS_BASE-t)));
        q = (q - t) / (BS_BASE - t);
      }
      buf_achar(buf, enc_digit(q));
      bias = adapt(delta, h+1, h == b);
      delta = 0;
      h++;
      remaining--;
    }
    delta++;
    n++;
  }
  return 0;
}

char *punycode_encode(const void *in, size_t len) {
  const uint8_t *curr = in, *prev = in;
  buf_t outbuf;
  buf_t lblbuf;
  int is_ascii = 1;

  memset(&outbuf, 0, sizeof(outbuf));
  memset(&lblbuf, 0, sizeof(lblbuf));
  if (buf_init(&outbuf, 512) == NULL) {
    goto fail;
  }

  if (buf_init(&lblbuf, 256) == NULL) {
    goto fail;
  }

  while (len > 0) {
    if (*curr == '\0') {
      goto fail;
    } else if (*curr == '.' && prev < curr) {
      if (is_ascii) {
        buf_adata(&outbuf, prev, curr-prev);
      } else {
        if (encode_label(&lblbuf, prev, curr-prev) < 0) {
          goto fail;
        }
        buf_adata(&outbuf, lblbuf.data, lblbuf.len);
        buf_clear(&lblbuf);
      }
      buf_achar(&outbuf, '.');
      is_ascii = 1;
      prev = curr+1;
    } else if (*curr > 0x7f) {
      is_ascii = 0;
    }
    curr++;
    len--;
  }

  /* last label */
  if (prev < curr) {
    if (is_ascii) {
      buf_adata(&outbuf, prev, curr-prev);
    } else {
      if (encode_label(&lblbuf, prev, curr-prev) < 0) {
        goto fail;
      }
      buf_adata(&outbuf, lblbuf.data, lblbuf.len);
      buf_clear(&lblbuf);
    }
  }

  buf_achar(&outbuf, '\0');
  buf_cleanup(&lblbuf);
  return outbuf.data;

fail:
  buf_cleanup(&outbuf);
  buf_cleanup(&lblbuf);
  return NULL;
}

#ifdef WOLOLO

#include <stdio.h>
#include <stdlib.h>

int main() {
  struct {
    char *in;
    char *expected;
  } vals[] = {
    {"", ""},
    {".", "."},
    {"..", ".."},
    {"...", "..."},
    {"www.example.com", "www.example.com"},
    {"bücher.example.com", "xn--bcher-kva.example.com"},
    {NULL, NULL},
  };
  size_t i;

  for(i=0; vals[i].in != NULL; i++) {
    char *actual = punycode_encode(vals[i].in, strlen(vals[i].in));
    printf("\"%s\" \"%s\"\n", vals[i].expected, actual);
    if (actual != NULL) {
      free(actual);
    }
  }
}

#endif
