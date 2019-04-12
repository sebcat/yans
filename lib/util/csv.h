#ifndef YANS_CSV_H__
#define YANS_CSV_H__

/* RFC4180 */

#include <stdio.h>

#include <lib/util/buf.h>

struct csv_reader {
  buf_t data;
  buf_t cols;
};

static inline size_t csv_reader_nelems(struct csv_reader *r) {
  return r->cols.len / sizeof(size_t);
}

static inline const char *csv_reader_elem(struct csv_reader *r, size_t i) {
  size_t len;
  size_t *offs;

  len = csv_reader_nelems(r);
  if (i >= len) {
    return NULL;
  } else {
    offs = (size_t*)r->cols.data;
    return r->data.data + offs[i];
  }
}

int csv_encode(buf_t *dst, const char **cols, size_t ncols);

int csv_reader_init(struct csv_reader *r);
void csv_reader_cleanup(struct csv_reader *r);
int csv_read_row(struct csv_reader *r, FILE *in);

#endif
