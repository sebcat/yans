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
#ifndef BUF_H_
#define BUF_H_

#include <stddef.h>

/* buf_t --
 *   growable memory buffer
 *
 *   NB: a user may read the fields in here, but 'data' may change if
 *       buf_achar or buf_adata is called, so it's important to not keep
 *       pointers into 'data' efter calls to buf_achar or buf_adata
 */
typedef struct buf_t {
  size_t cap; /* capacity: size of memory buffer */
  size_t len; /* length:   used area of memory buffer, in bytes */
  char *data; /* data:     the actual memory buffer */
} buf_t;

#define BUF_ALIGNMENT    sizeof(long)    /* must be a power of two */

#define buf_clear(buf__) ((buf__)->len = 0)

#define buf_shrink(buf__, nbytes__) \
  (buf__)->len = ((nbytes__) > (buf__)->len) ? 0 : (buf__)->len - (nbytes__);

#define buf_truncate(buf__, off__)         \
  if ((off__) < (buf__)->len) {            \
    (buf__)->len = (off__);                \
  }                                        \

#define buf_align_offset(x) \
    (((x)+(BUF_ALIGNMENT-1))&~(BUF_ALIGNMENT-1))

buf_t *buf_init(buf_t *buf, size_t cap);
void buf_cleanup(buf_t *buf);
int buf_grow(buf_t *buf, size_t needed);
int buf_align(buf_t *buf);
int buf_alloc(buf_t *buf, size_t len, size_t *offset);
int buf_adata(buf_t *buf, const void *data, size_t len);

static inline int buf_reserve(buf_t *buf, size_t nbytes) {
  size_t nleft;

  nleft = buf->cap - buf->len;
  if (nleft < nbytes && buf_grow(buf, nbytes - nleft) < 0) {
    return -1;
  }

  return 0;
}

static inline int buf_achar(buf_t *buf, int ch) {
  int ret;

  if ((ret = buf_reserve(buf, 1)) == 0) {
    buf->data[buf->len++] = (char)ch;
  }

  return ret;
}
#endif
