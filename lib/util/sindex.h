#ifndef UTIL_SINDEX_H__
#define UTIL_SINDEX_H__

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define SINDEX_IDSZ        20
#define SINDEX_NAMESZ      44
#define SINDEX_MAGIC    0x29a

enum sindex_error {
  SINDEX_ESTAT,
  SINDEX_EWRITE,
  SINDEX_EBADINDEX,
  SINDEX_ESEEK,
};

struct sindex_ctx {
  /* internal */
  FILE *fp;
  enum sindex_error err;
  int oerrno;
};

struct sindex_entry {
  time_t indexed;
  char id[SINDEX_IDSZ];
  char name[SINDEX_NAMESZ];
  uint16_t magic;
  uint16_t reserved0;
};

#define sindex_init(s__, fp__) \
  (s__)->fp = (fp__), (s__)->err = 0

#define sindex_fp(s__) \
  (s__)->fp

/* fp must be opened with a+ or similar. Returns -1 on failure, 0 on success */
ssize_t sindex_put(struct sindex_ctx *ctx, struct sindex_entry *e);

/* returns -1 on error, the number of actually read entries on success.
 * if 'beforë́' is 0 - the elements are read from the end. If not NULL, 'last'
 * receives the index of the last element. The elements in elems will be
 * in reverse insertion order. fp must be readable */
ssize_t sindex_get(struct sindex_ctx *ctx, struct sindex_entry *elems,
    size_t nelems, size_t before, size_t *last);

void sindex_geterr(struct sindex_ctx *ctx, char *data, size_t len);
    

#endif
