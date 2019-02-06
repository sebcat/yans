/* linvar --
 *   Allocates blocks of memory using mmap(2) and uses a linear allocator
 *   within each block for allocating variably sized elements, called
 *   chunks. All memory is freed when linvar_cleanup is called.
 */
#ifndef ALLOC_LINVAR_H__
#define ALLOC_LINVAR_H__

#include <stddef.h>

struct linvar_chunk {
  unsigned int len;
  unsigned char data[];
};

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
struct linvar_chunk *linvar_alloc(struct linvar_ctx *ctx,
    size_t len);

#endif
