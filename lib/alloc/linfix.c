#include <stdlib.h>
#include <stdio.h>

#include <lib/alloc/linfix.h>

void linfix_cleanup(struct linfix_ctx *ctx) {
  struct linfix_block *curr;
  struct linfix_block *next;

  if (ctx) {
    curr = ctx->blks;
    while (curr != NULL) {
      next = curr->next;
      free(curr);
      curr = next;
    }
  }
}

void *linfix_alloc(struct linfix_ctx *ctx) {
  struct linfix_opts *opts;
  struct linfix_block *blk;
  unsigned char *dataptr;
  size_t size;

  opts = &ctx->opts;
  if (ctx->blks == NULL || ctx->blks->used == opts->nobjs) {
    size = sizeof(struct linfix_block) + opts->nobjs * opts->objsize;
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
