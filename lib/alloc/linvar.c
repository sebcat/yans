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
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <limits.h>
#include <string.h>

#include <lib/alloc/linvar.h>
#include <lib/util/macros.h>

#ifdef LINVAR_DBG
/* malloc(3) backed implementation for debugging using valgrind or ASAN */

void linvar_init(struct linvar_ctx *ctx, size_t blksize) {
  memset(ctx, 0, sizeof(*ctx));
}

void linvar_cleanup(struct linvar_ctx *ctx) {
  struct linvar_block *curr;
  struct linvar_block *next;

  if (ctx) {
    curr = ctx->blks;
    while (curr != NULL) {
      next = curr->next;
      free(curr);
      curr = next;
    }
  }
}

void *linvar_alloc(struct linvar_ctx *ctx, size_t len) {
  struct linvar_block *blk;
  size_t newlen;

  newlen = len + sizeof(struct linvar_block);
  if (newlen < len) {
    /* Overflow */
    return NULL;
  }

  blk = malloc(newlen);
  if (blk == NULL) {
    return NULL;
  }

  memset(blk, 0, newlen);
  blk->next = ctx->blks;
  ctx->blks  = blk;
  return blk->chunks;
}

#else /* LINVAR_DBG */

/* Alignment must be powers of two */
#define CHUNK_ALIGNMENT   (1 << 2)
#define DFL_BLK_ALIGNMENT (1 << 12)

void linvar_init(struct linvar_ctx *ctx, size_t blksize) {
  long pagesz;
  size_t blkmask;

  /* Assume page size is a power of two */
  pagesz = sysconf(_SC_PAGESIZE);
  if (pagesz <= 0) {
    pagesz = DFL_BLK_ALIGNMENT;
  }

  blkmask = (size_t)pagesz - 1;
  ctx->blksize = (blksize + blkmask) & ~(blkmask);
  ctx->blkmask = blkmask;
  ctx->blks = NULL;
}

void linvar_cleanup(struct linvar_ctx *ctx) {
  struct linvar_block *curr;
  struct linvar_block *next;

  if (ctx) {
    curr = ctx->blks;
    while (curr != NULL) {
      next = curr->next;
      munmap(curr, curr->cap + sizeof(struct linvar_block));
      curr = next;
    }
  }
}

void *linvar_alloc(struct linvar_ctx *ctx, size_t len) {
  size_t chunk_needed;
  size_t blk_needed;
  struct linvar_block *blk;
  void *chunk;

  /* Make sure len is valid. The chunk header uses unsigned ints which may
   * be the same size as size_t. On such platforms, this check may
   * result in a compilation warning. Fix it for those platforms */
  if (len == 0 || len > UINT_MAX) {
    return NULL;
  }

  /* Calculate the number of bytes needed for this chunk. */
  chunk_needed = (len + CHUNK_ALIGNMENT - 1) & ~(CHUNK_ALIGNMENT-1);

  /* Allocate a new block for this chunk if:
   *   1) No blocks are allocated, or
   *   2) This chunk does not fit in the current block */
  if (ctx->blks == NULL ||
      (ctx->blks->cap - ctx->blks->offset) < chunk_needed) {

    /* A block is the size of:
     *   1) The memory needed to contain an allocation, or
     *   2) The blocksize set up on init
     * whichever is higher */
    blk_needed = (sizeof(struct linvar_block) + chunk_needed +
        ctx->blkmask) & ~(ctx->blkmask);
    if (ctx->blksize > blk_needed) {
      blk_needed = ctx->blksize;
    }

    /* Allocate the block */
    blk = mmap(NULL, blk_needed, PROT_READ|PROT_WRITE,
        MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    if (blk == MAP_FAILED) {
      return NULL;
    }

    /* Initialize the fields of the newly allocated block and set it as
     * the current one */
    blk->next = ctx->blks;
    blk->cap = blk_needed - sizeof(struct linvar_block);
    blk->offset = 0;
    ctx->blks = blk;
  }

  blk = ctx->blks;
  chunk = blk->chunks + blk->offset;
  blk->offset += chunk_needed;
  return chunk;
}

char *linvar_strdup(struct linvar_ctx *ctx, const char *s) {
  size_t len;
  char *res;

  len = strlen(s) + 1;
  res = linvar_alloc(ctx, len);
  if (res == NULL) {
    return NULL;
  }

  memcpy(res, s, len);
  return res;
}

#endif /* LINVAR_DBG */
