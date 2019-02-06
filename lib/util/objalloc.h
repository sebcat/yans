/* objalloc --
 *   Allocates blocks of memory using mmap(2) and uses a linear allocator
 *   within each block for allocating variably sized elements, called
 *   chunks. All memory is freed when objalloc_cleanup is called.
 */
#ifndef OBJALLOC_H__
#define OBJALLOC_H__

#include <stddef.h>

struct objalloc_chunk {
  unsigned int len;
  unsigned char data[];
};

struct objalloc_block {
  struct objalloc_block *next; /* pointer to the next block, if any */
  size_t offset;
  size_t cap;
  unsigned char chunks[];
};

struct objalloc_ctx {
  size_t blksize;
  size_t blkmask;
  struct objalloc_block *blks;
};

void objalloc_init(struct objalloc_ctx *ctx, size_t blksize);
void objalloc_cleanup(struct objalloc_ctx *ctx);
struct objalloc_chunk *objalloc_alloc(struct objalloc_ctx *ctx,
    size_t len);

#endif
