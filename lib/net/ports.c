#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <lib/net/ports.h>

#define IS_PORT_RANGE_SEP(ch) \
    ((ch) == '\r' ||         \
     (ch) == '\n' ||         \
     (ch) == '\t' ||         \
     (ch) == ' '  ||         \
     (ch) == ',')

static int rangecmp(const void *p0, const void *p1) {
  const struct port_range *r0 = p0;
  const struct port_range *r1 = p1;

  if (r0->start == r1->start) {
    return r0->end - r1->end;
  } else {
    return r0->start - r1->start;
  }
}

/* NB: assumes rs is sorted */
static void compress_ranges(struct port_ranges *rs) {
  size_t curr;
  size_t next;

  for (curr = 0, next = 1; next < rs->nranges; next++) {
    if (rs->ranges[curr].end + 1 >= rs->ranges[next].start) {
      if (rs->ranges[next].end > rs->ranges[curr].end) {
        rs->ranges[curr].end = rs->ranges[next].end;
      }
    } else {
      curr++;
    }
  }

  rs->nranges = curr + 1;
}

int port_ranges_from_str(struct port_ranges *rs, const char *s,
    size_t *fail_off) {
  char ch;
  uint16_t tmp;
  const char *start = s;
  int result = -1;
  unsigned int num;
  buf_t buf;
  struct port_range range = {0};
  enum {
    SKIP_SEP,
    PARSE_LHS,
    PARSE_RHS,
  } S = SKIP_SEP;

  memset(rs, 0, sizeof(*rs));
  if (s == NULL || *s == '\0') {
    return 0;
  }

  if (buf_init(&buf, 256 * sizeof(struct port_range)) == NULL) {
    return -1;
  }

  while (*s != '\0') {
    ch = *s;
    switch (S) {
    case SKIP_SEP:
      if (IS_PORT_RANGE_SEP(ch)) {
        break;
      }
      num = 0;
      S = PARSE_LHS;
      /* fallthrough */
    case PARSE_LHS:
      if (ch == '-' || IS_PORT_RANGE_SEP(ch)) {
        range.start = (uint16_t)num;
        num = 0;
        if (ch == '-') {
          S = PARSE_RHS;
        } else {
          range.end = range.start;
          if (range.end < range.start) {
            tmp = range.start;
            range.start = range.end;
            range.end = tmp;
          }
          buf_adata(&buf, &range, sizeof(range));
          range.start = range.end = 0;
          S = SKIP_SEP;
        }
        break;
      } else if (ch < '0' || ch > '9') {
        goto fail;
      }
      num = num * 10 + (ch - '0');
      if (num > 65535) {
        goto fail;
      }
      break;
    case PARSE_RHS:
      if (IS_PORT_RANGE_SEP(ch)) {
        range.end = (uint16_t)num;
        if (range.end < range.start) {
          tmp = range.start;
          range.start = range.end;
          range.end = tmp;
        }
        num = 0;
        buf_adata(&buf, &range, sizeof(range));
        range.start = range.end = 0;
        S = SKIP_SEP;
        break;
      } else if (ch < '0' || ch > '9') {
        goto fail;
      }
      num = num * 10 + (ch - '0');
      if (num > 65535) {
        goto fail;
      }
      break;
    }

    s++;
  }

  /* append last element, if any */
  if (S == PARSE_LHS) {
    range.start = range.end = (uint16_t)num;
    if (range.end < range.start) {
      tmp = range.start;
      range.start = range.end;
      range.end = tmp;
    }
    buf_adata(&buf, &range, sizeof(range));
  } else if (S == PARSE_RHS) {
    range.end = (uint16_t)num;
    if (range.end < range.start) {
      tmp = range.start;
      range.start = range.end;
      range.end = tmp;
    }
    buf_adata(&buf, &range, sizeof(range));
  }

  if (buf.len >= sizeof(range)) {
    rs->nranges = buf.len / sizeof(range);
    rs->ranges = (struct port_range*)buf.data; /* rs takes ownership */
  } else {
    /* empty result */
    buf_cleanup(&buf);
    return 0;
  }

  qsort(rs->ranges, rs->nranges, sizeof(struct port_range), rangecmp);
  compress_ranges(rs);
  return 0;

fail:
  if (fail_off != NULL) {
    *fail_off = s - start;
  }
  buf_cleanup(&buf);
  return result;
}

static int append_range(buf_t *buf, struct port_range *r, const char *sep) {
  char pad[16];
  int len;

  if (r->start == r->end) {
    len = snprintf(pad, sizeof(pad), "%s%hu", sep, r->start);
  } else {
    len = snprintf(pad, sizeof(pad), "%s%hu-%hu", sep, r->start, r->end);
  }

  if (len <= 0 || len >= sizeof(pad)) {
    return -1;
  }

  if (buf_adata(buf, pad, (size_t)len) < 0) {
    return -1;
  }

  return 0;
}

int port_ranges_to_buf(struct port_ranges *rs, buf_t *buf) {
  size_t i;

  buf_clear(buf);

  if (rs->nranges == 0) {
    return 0;
  }

  if (append_range(buf, &rs->ranges[0], "") < 0) {
    return -1;
  }


  for (i = 1; i < rs->nranges; i++) {
    if (append_range(buf, &rs->ranges[i], " ") < 0) {
      return -1;
    }
  }

  return 0;
}

void port_ranges_cleanup(struct port_ranges *rs) {
  if (rs != NULL) {
    if (rs->ranges != NULL) {
      free(rs->ranges);
    }
    rs->nranges = 0;
    rs->curr = 0;
  }

}

int port_ranges_next(struct port_ranges *rs, struct port_range *out) {
  return 0;
}
