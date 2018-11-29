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

struct idtbl_table {
  /* internal */
  uint32_t cap;  /* total number of entries (even power of two) */
  uint32_t size; /* number of entries in use */
  uint32_t hashseed; /* seed value for hash */
  uint32_t modmask; /* bitmask used for power-of-two modulus */
  struct idtbl_entry entries[];
};

struct idtbl_ctx {
  /* internal */
  struct idtbl_table *tbl;
  uint32_t rehash_limit;
};

#define idtbl_size(ctx__) ((ctx__)->tbl->size)

int idtbl_init(struct idtbl_ctx *ctx, uint32_t nslots, uint32_t seed);
void idtbl_cleanup(struct idtbl_ctx *ctx);
int idtbl_get(struct idtbl_ctx *ctx, uint32_t key, void **value);
int idtbl_contains(struct idtbl_ctx *ctx, uint32_t key);
int idtbl_remove(struct idtbl_ctx *ctx, uint32_t key);
int idtbl_insert(struct idtbl_ctx *ctx, uint32_t key, void *value);
const char *idtbl_strerror(int code);

#endif
