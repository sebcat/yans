#include "lib/vulnspec/vulnspec.h"

#define DEFAULT_PROGN_SIZE (512 * 1024)

int vulnspec_progn_init(struct vulnspec_progn *progn) {
  if (!buf_init(&progn->buf, DEFAULT_PROGN_SIZE)) {
    return -1;
  }

  return 0;
}

void vulnspec_progn_cleanup(struct vulnspec_progn *progn) {
  buf_cleanup(&progn->buf);
}

int vulnspec_progn_alloc(struct vulnspec_progn *progn,
    size_t len, struct vulnspec_value *out) {
  int i;
  size_t off;

  i = buf_alloc(&progn->buf, len, &off);
  if (i == 0) {
    i = buf_align(&progn->buf);
  }

  if (off > UINT32_MAX) {
    return -1;
  }

  out->offset = (uint32_t)off;
  return i;
}

void *vulnspec_progn_deref(struct vulnspec_progn *progn,
    struct vulnspec_value *val, size_t len) {
  size_t endoff;

  endoff = val->offset + len;
  if (endoff < val->offset) { /* integer overflow check */
    return NULL;
  }

  if (endoff > progn->buf.len) { /* buffer overflow check */
    return NULL;
  }

  return vulnspec_progn_deref_unsafe(progn, val);
}

