#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <limits.h>

#include <lib/util/objalloc.h>
#include <lib/util/macros.h>

/* Alignment must be powers of two */
#define CHUNK_ALIGNMENT   (1 << 2)
#define DFL_BLK_ALIGNMENT (1 << 12)

void objalloc_init(struct objalloc_ctx *ctx, size_t blksize) {
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

void objalloc_cleanup(struct objalloc_ctx *ctx) {
  struct objalloc_block *curr;
  struct objalloc_block *next;

  if (ctx) {
    curr = ctx->blks;
    while (curr != NULL) {
      next = curr->next;
      munmap(curr, curr->cap + sizeof(struct objalloc_block));
      curr = next;
    }
  }
}

struct objalloc_chunk *objalloc_alloc(struct objalloc_ctx *ctx,
    size_t len) {
  size_t chunk_needed;
  size_t blk_needed;
  struct objalloc_block *blk;
  struct objalloc_chunk *chunk;

  /* Make sure len is valid. The chunk header uses unsigned ints which may
   * be the same size as size_t. On such platforms, this check may
   * result in a compilation warning. Fix it for those platforms */
  if (len == 0 || len > UINT_MAX) {
    return NULL;
  }

  /* Calculate the number of bytes needed for this chunk. */
  chunk_needed = (sizeof(struct objalloc_chunk) + len + CHUNK_ALIGNMENT - 1)
      & ~(CHUNK_ALIGNMENT-1);

  /* Allocate a new block for this chunk if:
   *   1) No blocks are allocated, or
   *   2) This chunk does not fit in the current block */
  if (ctx->blks == NULL ||
      (ctx->blks->cap - ctx->blks->offset) < chunk_needed) {

    /* A block is the size of:
     *   1) The memory needed to contain an allocation, or
     *   2) The blocksize set up on init
     * whichever is higher */
    blk_needed = (sizeof(struct objalloc_block) + chunk_needed +
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
    blk->cap = blk_needed - sizeof(struct objalloc_block);
    blk->offset = 0;
    ctx->blks = blk;
  }

  blk = ctx->blks;
  chunk = (struct objalloc_chunk *)(blk->chunks + blk->offset);
  blk->offset += chunk_needed;
  return chunk;
}
