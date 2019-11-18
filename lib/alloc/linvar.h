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
/* linvar --
 *   Allocates blocks of memory using mmap(2) and uses a linear allocator
 *   within each block for allocating variably sized elements, called
 *   chunks. All allocated blocks are freed when linvar_cleanup is called.
 *
 *   If LINVAR_DBG is defined, a malloc(3)-based implementation is used
 *   instead, with one malloc(3) call issued per allocation. This
 *   is useful for debugging with tools like valgrind or ASan, but
 *   eliminates the benefits of the linear allocator.
 */
#ifndef ALLOC_LINVAR_H__
#define ALLOC_LINVAR_H__

#include <stddef.h>

struct linvar_block {
  struct linvar_block *next; /* pointer to the next block, if any */
  size_t offset;
  size_t cap;
  unsigned char chunks[];
};

struct linvar_ctx {
  size_t blksize;
  size_t blkmask;
  struct linvar_block *blks;
};

void linvar_init(struct linvar_ctx *ctx, size_t blksize);
void linvar_cleanup(struct linvar_ctx *ctx);
void *linvar_alloc(struct linvar_ctx *ctx, size_t len);
char *linvar_strdup(struct linvar_ctx *ctx, const char *s);

#endif
