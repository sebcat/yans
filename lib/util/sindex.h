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
#ifndef UTIL_SINDEX_H__
#define UTIL_SINDEX_H__

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define SINDEX_IDSZ        20
#define SINDEX_NAMESZ      44
#define SINDEX_MAGIC    0x29a

enum sindex_error {
  SINDEX_ESTAT,
  SINDEX_EWRITE,
  SINDEX_EBADINDEX,
  SINDEX_ESEEK,
};

struct sindex_ctx {
  /* internal */
  FILE *fp;
  enum sindex_error err;
  int oerrno;
};

struct sindex_entry {
  time_t indexed;
  char id[SINDEX_IDSZ];
  char name[SINDEX_NAMESZ];
  uint16_t magic;
  uint16_t reserved0;
};

#define sindex_init(s__, fp__) \
  (s__)->fp = (fp__), (s__)->err = 0

#define sindex_fp(s__) \
  (s__)->fp

/* fp must be opened with a+ or similar. Returns -1 on failure, 0 on success */
ssize_t sindex_put(struct sindex_ctx *ctx, struct sindex_entry *e);

/* returns -1 on error, the number of actually read entries on success.
 * if 'beforë́' is 0 - the elements are read from the end. If not NULL, 'last'
 * receives the index of the last element. The elements in elems will be
 * in reverse insertion order. fp must be readable */
ssize_t sindex_get(struct sindex_ctx *ctx, struct sindex_entry *elems,
    size_t nelems, size_t before, size_t *last);

void sindex_geterr(struct sindex_ctx *ctx, char *data, size_t len);
    

#endif
