#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include <lib/net/ports.h>

#define IS_PORT_RANGE_SEP(ch) \
    ((ch) == '\r' ||         \
     (ch) == '\n' ||         \
     (ch) == '\t' ||         \
     (ch) == ' '  ||         \
     (ch) == ',')

#define PORT_RANGE_HAS_LHS (1 << 0)
#define PORT_RANGE_HAS_RHS (1 << 1)

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
static size_t compress_ranges(struct port_ranges *rs) {
  size_t curr;
  size_t next;

  for (curr = 0, next = 1; next < rs->nranges; next++) {
    if (rs->ranges[curr].end + 1 >= rs->ranges[next].start) {
      if (rs->ranges[next].end > rs->ranges[curr].end) {
        rs->ranges[curr].end = rs->ranges[next].end;
      }
    } else {
      curr++;
      if (next > curr) {
        memmove(rs->ranges + curr, rs->ranges + next,
            (rs->nranges - next) * sizeof(struct port_range));
        next = curr;
      }
    }
  }

  return curr + 1;
}

int port_ranges_from_str(struct port_ranges *rs, const char *s,
    size_t *fail_off) {
  char ch;
  uint16_t tmp;
  const char *start = s;
  int result = -1;
  unsigned int num = 0;
  unsigned int flags = 0;
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
      flags = 0;
      S = PARSE_LHS;
      /* fallthrough */
    case PARSE_LHS:
      if (ch == '-' || IS_PORT_RANGE_SEP(ch)) {
        range.start = (uint16_t)num;
        if (ch == '-') {
          if (!(flags & PORT_RANGE_HAS_LHS)) {
            /* dash without left hand side of range is invalid */
            goto fail;
          }
          num = 0;
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
      flags |= PORT_RANGE_HAS_LHS;
      num = num * 10 + (ch - '0');
      if (num > 65535) {
        goto fail;
      }
      break;
    case PARSE_RHS:
      if (IS_PORT_RANGE_SEP(ch)) {
        if (!(flags & PORT_RANGE_HAS_RHS)) {
          /* dash without right hand side of range is invalid */
          goto fail;
        }
        range.end = (uint16_t)num;
        if (range.end < range.start) {
          tmp = range.start;
          range.start = range.end;
          range.end = tmp;
        }
        buf_adata(&buf, &range, sizeof(range));
        range.start = range.end = 0;
        S = SKIP_SEP;
        break;
      } else if (ch < '0' || ch > '9') {
        goto fail;
      }
      flags |= PORT_RANGE_HAS_RHS;
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
    if (!(flags & PORT_RANGE_HAS_RHS)) {
      goto fail;
    }
    range.end = (uint16_t)num;
    if (range.end < range.start) {
      tmp = range.start;
      range.start = range.end;
      range.end = tmp;
    }
    buf_adata(&buf, &range, sizeof(range));
  }

  /* if we have a range, set up rs. If we don't, return an empty result */
  if (buf.len >= sizeof(range)) {
    rs->cap = buf.cap / sizeof(range);
    rs->nranges = buf.len / sizeof(range);
    rs->ranges = (struct port_range*)buf.data; /* rs takes ownership */
  } else {
    buf_cleanup(&buf);
    return 0;
  }

  /* sort and compress the ranges */
  qsort(rs->ranges, rs->nranges, sizeof(struct port_range), rangecmp);
  rs->nranges = compress_ranges(rs);
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
  struct port_range *r;
  size_t i;
  uint16_t swp;
  int ret;

  buf_clear(buf);

  /* we're iterating, and we've reached the end */
  if (rs->curr_range >= rs->nranges) {
    return 0;
  }

  r = rs->ranges + rs->curr_range;
  if (rs->curr_range == 0 && rs->curr_port == 0) {
    /* first entry - initialize rs->curr_port to 0, which may also be 0 */
    rs->curr_port = r->start;
  }

  swp = r->start;
  r->start = rs->curr_port;
  ret = append_range(buf, r, "");
  r->start = swp;
  if (ret < 0) {
    return -1;
  }


  for (i = rs->curr_range + 1; i < rs->nranges; i++) {
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
      rs->ranges = NULL;
    }
    rs->nranges = 0;
    rs->curr_range = 0;
    rs->curr_port = 0;
  }
}

int port_ranges_next(struct port_ranges *rs, uint16_t *out) {
  struct port_range *r;

  if (rs->curr_range >= rs->nranges) {
    /* we've reached the end of the iteration - reset counters and return 0 */
    port_ranges_reset(rs);
    return 0;
  }

  r = rs->ranges + rs->curr_range;
  if (rs->curr_range == 0 && rs->curr_port == 0) {
    /* first entry - initialize rs->curr_port to 0, which may also be 0 */
    rs->curr_port = r->start;
  }


  *out = rs->curr_port;
  if (r->end > rs->curr_port) {
    rs->curr_port++;
  } else {
    rs->curr_range++;
    r = rs->ranges + rs->curr_range;
    rs->curr_port = r->start;
  }
  return 1;
}

int port_ranges_add(struct port_ranges *dst, struct port_ranges *from) {
  size_t n;
  void *tmp;

  /* if we don't have anything to add - return */
  if (from->nranges == 0) {
    return 0;
  }

  /* allocate more space if needed */
  if (from->nranges > (dst->cap - dst->nranges)) {
    n = dst->cap + from->nranges;
    n += n / 2; /* grow by 50%  */
    tmp = realloc(dst->ranges, sizeof(struct port_range) * n);
    if (tmp == NULL) {
      return -1;
    }
    dst->cap = n;
    dst->ranges = tmp;
  }

  memcpy(dst->ranges + dst->nranges, from->ranges,
      from->nranges * sizeof(struct port_range));
  dst->nranges += from->nranges;
  qsort(dst->ranges, dst->nranges, sizeof(struct port_range), rangecmp);
  dst->nranges = compress_ranges(dst);
  return 0;
}

static void port_r4ranges_reset(struct port_r4ranges *r4) {
  uint32_t mapval;
  struct reorder32 mapgen;
  int i;

  /* initialize the lookup for the ranges */
  reorder32_init(&mapgen, 0, (uint32_t)r4->nranges - 1);
  for (i = 0; i < (int)r4->nranges; i++) {
    reorder32_next(&mapgen, &mapval);
    r4->rangemap[i] = (int)mapval;
  }

  /* initialize the ranges themselves */
  for (i = 0; i < (int)r4->nranges; i++) {
    reorder32_init(&r4->ranges[i],
        (uint32_t)r4->rs->ranges[i].start,
        (uint32_t)r4->rs->ranges[i].end);
  }
}

int port_r4ranges_init(struct port_r4ranges *r4, struct port_ranges *rs) {
  struct reorder32 *ranges;
  int *rangemap;

  r4->mapindex = 0;
  r4->nranges = rs->nranges;
  if (r4->nranges == 0 || r4->nranges > INT_MAX) {
    return 0;
  }
  r4->rs = rs;

  ranges = calloc(rs->nranges, sizeof(*ranges));
  if (ranges == NULL) {
    goto fail;
  }

  rangemap = calloc(rs->nranges, sizeof(*rangemap));
  if (rangemap == NULL) {
    goto cleanup_ranges;
  }

  r4->ranges = ranges;
  r4->rangemap = rangemap;
  port_r4ranges_reset(r4);
  return 0;

cleanup_ranges:
  free(ranges);
fail:
  return -1;
}


void port_r4ranges_cleanup(struct port_r4ranges *r4) {
  if (r4 != NULL) {
    if (r4->ranges != NULL) {
      free(r4->ranges);
      r4->ranges = NULL;
    }
    if (r4->rangemap != NULL) {
      free(r4->rangemap);
      r4->rangemap = NULL;
    }
    r4->nranges = 0;
  }
}

int port_r4ranges_next(struct port_r4ranges *r4, uint16_t *out) {
  size_t curr;
  size_t mapped_index;
  struct reorder32 *range;
  int ret;
  uint32_t res;

  if (r4->nranges == 0) {
    return 0;
  }

  /* get the next block index */
again:
  curr = r4->mapindex;
  do {
    curr = (curr + 1) % r4->nranges;
  } while(curr != r4->mapindex && r4->rangemap[curr] < 0);

  /* check if iterator is depleted */
  if (curr == r4->mapindex && r4->rangemap[curr] < 0) {
    port_r4ranges_reset(r4);
    return 0;
  }

  /* update mapindex with the new index and map the current range */
  r4->mapindex = curr;
  mapped_index = r4->rangemap[curr];
  range = &r4->ranges[mapped_index];

  ret = reorder32_next(range, &res);
  if (ret == 0) {
    /* mark range as depleted and try again */
    r4->rangemap[curr] = -1;
    goto again;
  }

  *out = (uint16_t)res;
  return 1;
}
