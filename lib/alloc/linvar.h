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
