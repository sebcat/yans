/* RFC3492 punycode */
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include <lib/util/u8.h>
#include <lib/net/punycode.h>

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

int punycode_init(struct punycode_ctx *ctx) {
  memset(ctx, 0, sizeof(*ctx));
  if (buf_init(&ctx->outbuf, 512) == NULL) {
    return -1;
  }

  if (buf_init(&ctx->lblbuf, 256) == NULL) {
    buf_cleanup(&ctx->outbuf);
    return -1;
  }

  return 0;
}

void punycode_cleanup(struct punycode_ctx *ctx) {
  buf_cleanup(&ctx->outbuf);
  buf_cleanup(&ctx->lblbuf);
}

char *punycode_encode(struct punycode_ctx *ctx, const char *in, size_t len) {
  const uint8_t *curr;
  const uint8_t *prev;
  int is_ascii = 1;

  curr = (const uint8_t*)in;
  prev = (const uint8_t*)in;
  buf_clear(&ctx->outbuf);
  buf_clear(&ctx->lblbuf);

  while (len > 0) {
    if (*curr == '\0') {
      return NULL;
    } else if (*curr == '.' && prev < curr) {
      if (is_ascii) {
        buf_adata(&ctx->outbuf, prev, curr-prev);
      } else {
        if (encode_label(&ctx->lblbuf, prev, curr-prev) < 0) {
          return NULL;
        }
        buf_adata(&ctx->outbuf, ctx->lblbuf.data, ctx->lblbuf.len);
        buf_clear(&ctx->lblbuf);
      }
      buf_achar(&ctx->outbuf, '.');
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
      buf_adata(&ctx->outbuf, prev, curr-prev);
    } else {
      if (encode_label(&ctx->lblbuf, prev, curr-prev) < 0) {
        return NULL;
      }
      buf_adata(&ctx->outbuf, ctx->lblbuf.data, ctx->lblbuf.len);
      buf_clear(&ctx->lblbuf);
    }
  }

  buf_achar(&ctx->outbuf, '\0');
  buf_cleanup(&ctx->lblbuf);
  return ctx->outbuf.data;
}

