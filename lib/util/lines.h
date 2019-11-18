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
#ifndef UTIL_LINES_H__
#define UTIL_LINES_H__

#include <stdio.h>

/* 'lines' is just a line buffering abstraction with the following ideas:
 *
 * - A chunk of lines is read at a time (lines_next_chunk)
 * - A line in a chunk is valid until the next chunk is read
 * - For the lifetime of a chunk, there's no need to copy a single line
 * - Lines are mutable
 */

/* status codes */
#define LINES_CONTINUE     1
#define LINES_OK           0
#define LINES_ENOMEM      -1
#define LINES_EREAD       -2

#define LINES_IS_ERR(x) (x < 0)

struct lines_ctx {
  /* internal */
  FILE *fp;
  size_t cap;     /* size of 'data' (excluding trailing \0 byte) */
  size_t len;     /* length of read actual data in 'data' */
  size_t end;     /* end of last complete line in 'data' */
  size_t lineoff; /* current line offset */
  char *data;     /* chunk of file */
};

int lines_init(struct lines_ctx *ctx, FILE *fp);
void lines_cleanup(struct lines_ctx *ctx);
int lines_next_chunk(struct lines_ctx *ctx);
int lines_next(struct lines_ctx *ctx, char **out, size_t *outlen);
const char *lines_strerror(int err);

#endif
