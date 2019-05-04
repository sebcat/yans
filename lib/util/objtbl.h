#ifndef YANS_OBJTBL_H__
#define YANS_OBJTBL_H__

#include <stdio.h>
#include <stdint.h>

#define OBJTBL_OK            0
#define OBJTBL_ENOTFOUND    -1
#define OBJTBL_EFULL        -2
#define OBJTBL_EINVAL       -3
#define OBJTBL_ENOSLOT      -4
#define OBJTBL_ENOMEM       -5

typedef uint32_t objtbl_hash_t;

struct objtbl_entry {
  /* internal */
  objtbl_hash_t hash; /* cached hash of the value */
  int32_t distance;   /* # of elements from wanted slot to actual slot */
  void *value;
};

struct objtbl_opts {
  objtbl_hash_t hashseed; /* seed value for hash */
  objtbl_hash_t (*hashfunc)(const void * /* obj */, objtbl_hash_t /* seed */);
  int (*cmpfunc)(const void * /* key */, const void * /* entry */);
};

struct objtbl_header {
  /* internal */
  uint32_t cap;          /* total number of entries (even power of two) */
  uint32_t size;         /* number of entries in use */
  uint32_t modmask;      /* bitmask used for power-of-two modulus */
  uint32_t rehash_limit;
  struct objtbl_opts opts;
};

struct objtbl_ctx {
  /* internal */
  struct objtbl_header header;
  struct objtbl_entry *entries;
};

struct objtbl_stats {
  size_t nbytes; /* table size, in bytes */
  size_t size;   /* number of elements occupying slots */
  size_t cap;    /* total number of slots in current table */
  unsigned int mean_probe_distance;
  unsigned int max_probe_distance;
  double average_probe_distance;
};

#define objtbl_size(ctx__) ((ctx__)->header.size)
#define objtbl_cap(ctx__) ((ctx__)->header.cap)
#define objtbl_val(ctx__, i__) ((ctx__)->entries[(i__)].value)

int objtbl_init(struct objtbl_ctx *tbl, const struct objtbl_opts *opts,
    uint32_t nslots);
void objtbl_cleanup(struct objtbl_ctx *tbl);
int objtbl_get(struct objtbl_ctx *tbl, const void * keyobj, void **value);
int objtbl_contains(struct objtbl_ctx *tbl, void * keyobj);
int objtbl_remove(struct objtbl_ctx *tbl, void * keyobj);
int objtbl_insert(struct objtbl_ctx *tbl, void * obj);
int objtbl_copy(struct objtbl_ctx *dst, struct objtbl_ctx *src);
int objtbl_calc_stats(struct objtbl_ctx *tbl, struct objtbl_stats *result);
void objtbl_dump(struct objtbl_ctx *ctx, FILE *fp);

/* XXX: Destructive operation - use with care. Makes it impossible to
 *      use the get, contains, remove, insert functions. */
void objtbl_sort(struct objtbl_ctx *tbl);

const char *objtbl_strerror(int code);

/* default hash and compare for '\0'-terminated strings */
int objtbl_strcmp(const void *k, const void *e);
objtbl_hash_t objtbl_strhash(const void *obj, objtbl_hash_t seed);

#endif
