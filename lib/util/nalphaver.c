#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "lib/util/macros.h"

struct nalphaver_field {
  const char *pos;
  const char *suffix;
  int val;
  unsigned int suffixlen;
};

static int next(struct nalphaver_field *na) {
  unsigned long l;
  char *cptr;
  size_t off;
  const char *next;

  if (!*na->pos) {
    na->suffix = na->pos;
    na->val = na->suffixlen = 0;
    return 0;
  }

  l = strtoul(na->pos, &cptr, 10);
  l = MIN(l, INT_MAX);

  off = strcspn(cptr, ".");
  if (off > UINT_MAX) {
    off = UINT_MAX;
  }

  next = cptr + off;
  while (*next == '.') {
    next++;
  }

  na->suffix = cptr;
  na->suffixlen = off;
  na->val = (int)l;
  na->pos = next;
  return 1;
}

static int cmp(struct nalphaver_field *l, struct nalphaver_field *r) {
  int val;
  unsigned int len;

  val = l->val - r->val;
  if (val == 0) {
    len = MIN(l->suffixlen, r->suffixlen);
    val = strncmp(l->suffix, r->suffix, len);
    if (val == 0) {
      val = (int)l->suffixlen - (int)r->suffixlen;
    }
  }

  val = CLAMP(val, -1, 1);
  return val;
}

int nalphaver_cmp(const char *s1, const char *s2) {
  struct nalphaver_field left;
  struct nalphaver_field right;
  int r1;
  int r2;
  int val;

  left.pos = s1;
  right.pos = s2;

  r1 = next(&left);
  r2 = next(&right);
  while(r1 || r2) {
    val = cmp(&left, &right);
    if (val != 0) {
      return val;
    }

    r1 = next(&left);
    r2 = next(&right);
  }

  return 0;
}
