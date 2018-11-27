#ifndef YANS_IDTBL_H__
#define YANS_IDTBL_H__

#include <stdint.h>

#define IDTBL_OK            0
#define IDTBL_ENOTFOUND    -1
#define IDTBL_EFULL        -2
#define IDTBL_EINVAL       -3
#define IDTBL_ENOSLOT      -4

struct idtbl_entry {
  int32_t distance; /* # of elements from wanted slot to actual slot */
  int32_t key;
  void *value;
};

struct idtbl_table {
  uint32_t cap;  /* total number of entries (even power of two) */
  uint32_t size; /* number of entries in use */
  uint32_t max_distance; /* the longest distance of any inserted value */
  uint32_t hashseed; /* seed value for hash */
  struct idtbl_entry entries[];
};

/* NULL on failed allocation, non-NULL on success */
struct idtbl_table *idtbl_init(uint32_t nslots, uint32_t seed);
void idtbl_cleanup(struct idtbl_table *ctx);
int idtbl_get(struct idtbl_table *ctx, uint32_t key, void **value);
/* -1 on failed insertion (e.g., because table is full), 0 on success */
int idtbl_set(struct idtbl_table *ctx, uint32_t key, void *value);

#endif
