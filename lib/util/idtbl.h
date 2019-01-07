#ifndef YANS_IDTBL_H__
#define YANS_IDTBL_H__

#include <stdint.h>

#define IDTBL_OK            0
#define IDTBL_ENOTFOUND    -1
#define IDTBL_EFULL        -2
#define IDTBL_EINVAL       -3
#define IDTBL_ENOSLOT      -4
#define IDTBL_ENOMEM       -5

struct idtbl_entry {
  /* internal */
  int32_t distance; /* # of elements from wanted slot to actual slot */
  int32_t key;
  void *value;
};

struct idtbl_header {
  /* internal */
  uint32_t cap;  /* total number of entries (even power of two) */
  uint32_t size; /* number of entries in use */
  uint32_t hashseed; /* seed value for hash */
  uint32_t modmask; /* bitmask used for power-of-two modulus */
  uint32_t rehash_limit;
};

struct idtbl_ctx {
  /* internal */
  struct idtbl_header header;
  struct idtbl_entry *entries;
};

struct idtbl_stats {
  size_t nbytes; /* table size, in bytes */
  size_t size;   /* number of elements occupying slots */
  size_t cap;    /* total number of slots in current table */
  unsigned int mean_probe_distance;
  unsigned int max_probe_distance;
  double average_probe_distance;
};

#define idtbl_size(ctx__) ((ctx__)->header.size)

int idtbl_init(struct idtbl_ctx *ctx, uint32_t nslots, uint32_t seed);
void idtbl_cleanup(struct idtbl_ctx *ctx);
int idtbl_get(struct idtbl_ctx *ctx, uint32_t key, void **value);
int idtbl_contains(struct idtbl_ctx *ctx, uint32_t key);
int idtbl_remove(struct idtbl_ctx *ctx, uint32_t key);
int idtbl_insert(struct idtbl_ctx *ctx, uint32_t key, void *value);
int idtbl_copy(struct idtbl_ctx *dst, struct idtbl_ctx *src);
int idtbl_calc_stats(struct idtbl_ctx *ctx, struct idtbl_stats *result);

const char *idtbl_strerror(int code);

#endif
