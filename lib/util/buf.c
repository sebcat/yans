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
#include <stdlib.h>
#include <string.h>
#include <lib/util/buf.h>

buf_t *buf_init(buf_t *buf, size_t cap) {
  size_t acap = buf_align_offset(cap);
  if ((buf->data = calloc(1, acap)) == NULL) {
    return NULL;
  }

  buf->cap = acap;
  buf->len = 0;
  return buf;
}

void buf_cleanup(buf_t *buf) {
  if (buf->data != NULL) {
    free(buf->data);
    buf->data = NULL;
  }

  buf->cap = 0;
  buf->len = 0;
}

int buf_grow(buf_t *buf, size_t needed) {
  size_t added;
  char *ndata;

  added = buf_align_offset(buf->cap >> 1);
  if (added < needed) {
    added = buf_align_offset(needed);
  }

  if ((ndata = realloc(buf->data, buf->cap + added)) == NULL) {
    return -1;
  }

  /* This memset is here for two reasons:
     1) If the buffer is serialized, we do not want to leak any data in
        padding for alignment
     2) If there's a bug using allocated but uninitialized memory, we want
        that memory to have a known value for ease of debugging */
  memset(ndata + buf->cap, 0, added);

  buf->cap += added;
  buf->data = ndata;
  return 0;
}

int buf_align(buf_t *buf) {
  size_t noff;
  int ret;

  noff = buf_align_offset(buf->len);
  if (noff >= buf->cap) {
    ret = buf_grow(buf, BUF_ALIGNMENT);
    if (ret < 0) {
      return -1;
    }
  }

  buf->len = noff;
  return 0;
}

int buf_adata(buf_t *buf, const void *data, size_t len) {
  int ret;

  if (len == 0) {
    return 0;
  }

  if ((ret = buf_reserve(buf, len)) == 0) {
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
  }

  return ret;
}

int buf_alloc(buf_t *buf, size_t len, size_t *offset) {
  int ret;
  size_t nlen;
  size_t off;

  if (len == 0) {
    return -1;
  }

  off = buf->len;
  if ((ret = buf_reserve(buf, len)) != 0) {
    return -1;
  }

  nlen = buf->len + len;
  if (nlen < buf->len) { /* overflow check */
    return -1;
  }

  buf->len = nlen;
  *offset = off;
  return 0;
}
