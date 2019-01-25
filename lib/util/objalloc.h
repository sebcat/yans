/* objalloc --
 *   Allocates blocks of memory using calloc(3) and uses a linear allocator
 *   within each block for allocating equally sized elements. Memory is
 *   freed once for all blocks. Memory is guaranteed to be zeroed. Caller
 *   deals with alignment.
 */
#ifndef OBJALLOC_H__
#define OBJALLOC_H__

#include <stddef.h>

struct objalloc_block {
  struct objalloc_block *next; /* pointer to the next block, if any */
  unsigned int used;           /* # of elements used in this block */
  unsigned char data[];
};

struct objalloc_opts {
  unsigned int nobjs;   /* # of elements per block */
  unsigned int objsize; /* # of bytes occupied by one element */
};

struct objalloc_ctx {
  struct objalloc_opts opts;
  struct objalloc_block *blks;
};

static inline void objalloc_init(struct objalloc_ctx *ctx,
    const struct objalloc_opts *opts) {
  ctx->opts = *opts;
  ctx->blks = NULL;
}

void objalloc_free(struct objalloc_ctx *ctx);
void *objalloc_alloc(struct objalloc_ctx *ctx);

#endif
