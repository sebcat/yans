/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
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
