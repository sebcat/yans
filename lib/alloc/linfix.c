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
  struct linfix_block *blk;
  unsigned char *dataptr;
  size_t size;

  if (ctx->blks == NULL || ctx->blks->used == ctx->nmemb) {
    size = sizeof(struct linfix_block) + ctx->nmemb * ctx->size;
    blk = calloc(1, size);
    if (blk == NULL) {
      return NULL;
    }

    blk->next = ctx->blks;
    ctx->blks = blk;
  }

  blk = ctx->blks;
  dataptr = blk->data + blk->used * ctx->size;
  blk->used++;
  return dataptr;
}
