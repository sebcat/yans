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

struct linfix_opts {
  unsigned int nobjs;   /* # of elements per block */
  unsigned int objsize; /* # of bytes occupied by one element */
};

struct linfix_ctx {
  struct linfix_opts opts;
  struct linfix_block *blks;
};

static inline void linfix_init(struct linfix_ctx *ctx,
    const struct linfix_opts *opts) {
  ctx->opts = *opts;
  ctx->blks = NULL;
}

void linfix_cleanup(struct linfix_ctx *ctx);
void *linfix_alloc(struct linfix_ctx *ctx);

#endif
