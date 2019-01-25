#include <stdlib.h>
#include <stdio.h>

#include <lib/util/objalloc.h>

void objalloc_cleanup(struct objalloc_ctx *ctx) {
  struct objalloc_block *curr;
  struct objalloc_block *next;

  if (ctx) {
    curr = ctx->blks;
    while (curr != NULL) {
      next = curr->next;
      free(curr);
      curr = next;
    }
  }
}

void *objalloc_alloc(struct objalloc_ctx *ctx) {
  struct objalloc_opts *opts;
  struct objalloc_block *blk;
  unsigned char *dataptr;
  size_t size;

  opts = &ctx->opts;
  if (ctx->blks == NULL || ctx->blks->used == opts->nobjs) {
    size = sizeof(struct objalloc_block) + opts->nobjs * opts->objsize;
    blk = calloc(1, size);
    if (blk == NULL) {
      return NULL;
    }

    blk->next = ctx->blks;
    ctx->blks = blk;
  }

  blk = ctx->blks;
  dataptr = blk->data + blk->used * ctx->opts.objsize;
  blk->used++;
  return dataptr;
}
