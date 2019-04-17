#ifndef YANS_DEDUPTBL_H__
#define YANS_DEDUPTBL_H__

#include <stddef.h>
#include <stdint.h>

#define DEDUPTBL_MAGIC 0x75646564

#define DEDUPTBL_MAX_ENTRIES 0x3fffffff

/* return values */
#define DEDUPTBL_OK       0
#define DEDUPTBL_EERRNO  -1
#define DEDUPTBL_ETOOBIG -2
#define DEDUPTBL_EMAGIC  -3
#define DEDUPTBL_ESIZE   -4
#define DEDUPTBL_EFULL   -5
#define DEDUPTBL_EEXIST  -6
#define DEDUPTBL_ENOSLOT -7
#define DEDUPTBL_EHEADER -8

struct deduptbl_id {
  union {
    uint8_t u8[20];
    uint32_t u32[5];
  } val;
};

struct deduptbl_entry {
  uint32_t distance;
  struct deduptbl_id id;
};

struct deduptbl_table {
  uint32_t magic;
  uint32_t nmax;      /* maximum number of entries allowed in the table */
  uint32_t nused;     /* number of entries in use */
  uint32_t cap_log2;  /* log2 of number of slots in the table */
  struct deduptbl_entry entries[];
};

struct deduptbl_ctx {
  struct deduptbl_table *table;
  size_t filesize;
  int saved_errno;
};

struct deduptbl_vec {
  const void *data;
  size_t len;
};

void deduptbl_id(struct deduptbl_id *id, const void *data, size_t len);
void deduptbl_idv(struct deduptbl_id *id, struct deduptbl_vec *vec,
    size_t len);

const char *deduptbl_strerror(struct deduptbl_ctx *ctx, int err);

int deduptbl_create(struct deduptbl_ctx *ctx, uint32_t max_entries,
    int fd);
int deduptbl_load(struct deduptbl_ctx *ctx, int fd);
void deduptbl_cleanup(struct deduptbl_ctx *ctx);
int deduptbl_contains(struct deduptbl_ctx *ctx, struct deduptbl_id *id);
int deduptbl_update(struct deduptbl_ctx *ctx, struct deduptbl_id *id);


#endif
