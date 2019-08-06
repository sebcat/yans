#include "lib/vulnmatch/vulnmatch.h"

#define DEFAULT_PROGN_SIZE (512 * 1024)

int vulnmatch_progn_init(struct vulnmatch_progn *progn) {
  if (!buf_init(&progn->buf, DEFAULT_PROGN_SIZE)) {
    return -1;
  }

  return 0;
}

void vulnmatch_progn_cleanup(struct vulnmatch_progn *progn) {
  buf_cleanup(&progn->buf);
}

int vulnmatch_progn_alloc(struct vulnmatch_progn *progn,
    size_t len, struct vulnmatch_value *out) {
  int i;

  i = buf_alloc(&progn->buf, len, &out->offset);
  if (i == 0) {
    i = buf_align(&progn->buf);
  }

  return i;
}

void *vulnmatch_progn_deref(struct vulnmatch_progn *progn,
    struct vulnmatch_value *val, size_t len) {
  size_t endoff;

  endoff = val->offset + len;
  if (endoff < val->offset) { /* integer overflow check */
    return NULL;
  }

  if (endoff > progn->buf.len) { /* buffer overflow check */
    return NULL;
  }

  return vulnmatch_progn_deref_unsafe(progn, val);
}

