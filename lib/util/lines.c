#include <stdlib.h>
#include <string.h>

#include <lib/util/lines.h>
#include <lib/util/zfile.h>

#define LINES_CHUNKSIZE (1024 * 128) /* number of bytes in a chunk */

int lines_init(struct lines_ctx *ctx, int fd, int flags) {
  FILE *fp;
  char *data;

  fp = (flags & LINES_FCOMPR) ? zfile_fdopen(fd, "rb") : fdopen(fd, "rb");
  if (fp == NULL) {
    return LINES_EOPEN;
  }

  data = malloc(LINES_CHUNKSIZE + 1); /* + 1 for trailing \0 byte */
  if (data == NULL) {
    fclose(fp);
    return LINES_ENOMEM;
  }

  ctx->fp = fp;
  ctx->cap = LINES_CHUNKSIZE;
  ctx->len = 0;
  ctx->end = 0;
  ctx->lineoff = 0;
  ctx->data = data;
  return LINES_OK;
}

void lines_cleanup(struct lines_ctx *ctx) {
  if (ctx) {
    if (ctx->fp) {
      fclose(ctx->fp);
      ctx->fp = NULL;
    }
    free(ctx->data);
    ctx->data = NULL;
    ctx->cap = ctx->len = ctx->end = ctx->lineoff = 0;
  }
}

int lines_next_chunk(struct lines_ctx *ctx) {
  char *curr;
  size_t len;
  size_t i;

  /* If we're at EOF - we're done */
  if (feof(ctx->fp)) {
    return LINES_OK;
  }

  /* Move partial trailing data to the beginning of the chunk buffer.
   *
   * 'end' is the offset to the end of the last line (the \n). If we have
   * an incomplete line in the buffer it starts at 'end' + 1, and it needs
   * to be moved to the beginning of the buffer. */
  if (ctx->end < ctx->len) {
    curr = ctx->data + ctx->end + 1;
    len = ctx->len - (ctx->end + 1);
    memmove(ctx->data, curr, len);
    ctx->data[len] = '\0';
    ctx->len = len;
  } else {
    ctx->data[0] = '\0';
    ctx->len = 0;
  }

  /* Read the next chunk of data and \0-terminate it */
  len = fread(ctx->data + ctx->len, 1, ctx->cap - ctx->len, ctx->fp);
  if (len == 0) {
    return ferror(ctx->fp) ? LINES_EREAD : LINES_OK;
  }
  ctx->len += len;
  ctx->data[ctx->len] = '\0';

  /* find the end of the last complete line */
  for (i = ctx->len - 1; i > 0; i--) {
    if (ctx->data[i] == '\n') {
      break;
    }
  }

  if (i == 0) {
    /* no newline found, assume either last line with no newline or
     * truncate a very long line */
    ctx->end = ctx->len;
  } else {
    ctx->end = i;
  }

  ctx->lineoff = 0;
  return LINES_CONTINUE;
}

int lines_next(struct lines_ctx *ctx, char **out, size_t *outlen) {
  size_t i;

  /* We're at the end of the chunk buffer - signal done */
  if (ctx->lineoff >= ctx->end) {
    return LINES_OK;
  }

  /* find the next newline - if any (OK for last line to not end w/ \n) */
  for (i = ctx->lineoff; i < ctx->end; i++) {
    if (ctx->data[i] == '\n') {
      break;
    }
  }

  ctx->data[i] = '\0'; /* may be end+1 - OK. We alloted that much space */
  *out = ctx->data + ctx->lineoff;
  if (outlen != NULL) {
    *outlen = i - ctx->lineoff;
  }
  ctx->lineoff = i + 1;/* may be end + 2, also OK due to a check above */
  return LINES_CONTINUE;
}

const char *lines_strerror(int err) {
  switch(err) {
  case LINES_CONTINUE:
    return "continue";
  case LINES_OK:
    return "ok";
  case LINES_EOPEN:
    return "open failure";
  case LINES_ENOMEM:
    return "no memory";
  case LINES_EREAD:
    return "read error";
  default:
    return "unknown error";
  }
}