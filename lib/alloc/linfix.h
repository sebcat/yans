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
/* linfix --
 *   Allocates blocks of memory using calloc(3) and uses a linear allocator
 *   within each block for allocating equally sized elements. Memory is
 *   freed once for all blocks. Memory is guaranteed to be zeroed. Caller
 *   deals with alignment.
 */
#ifndef ALLOC_LINFIX_H__
#define ALLOC_LINFIX_H__

#include <stddef.h>

struct linfix_block {
  struct linfix_block *next; /* pointer to the next block, if any */
  unsigned int used;           /* # of elements used in this block */
  unsigned char data[];
};

struct linfix_ctx {
  unsigned int size;   /* # of bytes occupied by one element */
  unsigned int nmemb;  /* # of elements per block */
  struct linfix_block *blks;
};

static inline void linfix_init(struct linfix_ctx *ctx, unsigned int size,
    unsigned int nmemb) {
  ctx->size = size;
  ctx->nmemb = nmemb;
  ctx->blks = NULL;
}

void linfix_cleanup(struct linfix_ctx *ctx);
void *linfix_alloc(struct linfix_ctx *ctx);

#endif
