#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <lib/util/macros.h>
#include <lib/util/sindex.h>

#define SETERR(ctx__, code__) \
    (ctx__)->err = (code__), (ctx__)->oerrno = 0

#define SETERR_ERRNO(ctx__, code__) \
    (ctx__)->err = (code__), (ctx__)->oerrno = errno

ssize_t sindex_put(struct sindex_ctx *ctx, struct sindex_entry *e) {
  size_t ret;

  assert(ctx != NULL);
  assert(e != NULL);
  assert(ctx->fp != NULL);

  e->magic = SINDEX_MAGIC;
  ret = fwrite(e, sizeof(*e), 1, ctx->fp);
  if (ret != 1) {
    SETERR_ERRNO(ctx, SINDEX_EWRITE);
    return -1;
  }

  ret = fflush(ctx->fp);
  if (ret != 0) {
    SETERR_ERRNO(ctx, SINDEX_EWRITE);
    return -1;
  }

  return 0;
}

static ssize_t get_nelems(struct sindex_ctx *ctx) {
  off_t off;
  int fd;
  int ret;
  struct stat st;

  fd = fileno(ctx->fp);
  ret = fstat(fd, &st);
  if (ret < 0) {
    SETERR_ERRNO(ctx, SINDEX_ESTAT);
    return -1;
  }

  off = st.st_size;
  if (((size_t)off % sizeof(struct sindex_entry)) != 0) {
    SETERR(ctx, SINDEX_EBADINDEX);
    return -1;
  }

  return (size_t)off / sizeof(struct sindex_entry);
}

ssize_t sindex_get(struct sindex_ctx *ctx, struct sindex_entry *elems,
    size_t nelems, size_t before, size_t *last) {
  ssize_t nelems_index;
  off_t off;
  size_t nread;
  int ret;

  /* 'before' is the beginning of the element past the last element we want to
   * read.
   *
   * in the following scenario, with nelems = 2:
   *
   *         0-> +-----------+          0-> +------------+
   *             |    11     |              |     11     |
   *             +-----------+       last-> +------------+
   *             |    22     |              |     22     |  => elems: (22,33)
   *             +-----------+              +------------+     last: 1
   *             |    33     |              |     33     |
   *    before-> +-----------+              +------------+
   *
   */

  /* get the total number of entries in the index */
  nelems_index = get_nelems(ctx);
  if (nelems_index < 0) {
    return -1;
  }

  if (before == 0) {
    /* invariant: set 'before' to the number of index entries (the end) */
    before = (size_t)nelems_index;
  } else {
    /* invariant: set 'before' to at most the total number of index entries */
    before = MIN(before, (size_t)nelems_index);
  }

  /* limit the number of elements to fetch to that of the 'before' offset */
  nelems = MIN(nelems, before);
  if (nelems == 0) {
    return 0; /* nothing to read */
  }

  /* seek and destr... read... */
  off = before - nelems;
  ret = fseeko(ctx->fp, off * sizeof(*elems), SEEK_SET);
  if (ret < 0) {
    SETERR_ERRNO(ctx, SINDEX_ESEEK);
    return -1;
  }

  nread = fread(elems, sizeof(*elems), nelems, ctx->fp);
  *last = before - nread;
  return (ssize_t)nread;
}

void sindex_geterr(struct sindex_ctx *ctx, char *data, size_t len) {
  switch (ctx->err) {
  case SINDEX_ESTAT:
    snprintf(data, len, "unable to stat file: %s", strerror(ctx->oerrno));
  case SINDEX_EWRITE:
    snprintf(data, len, "write error: %s", strerror(ctx->oerrno));
  case SINDEX_EBADINDEX:
    snprintf(data, len, "%s", "corrupt index");
  case SINDEX_ESEEK:
    snprintf(data, len, "seek failure: %s", strerror(ctx->oerrno));
  default:
    snprintf(data, len, "%s", "unknown failure");
  }
}
