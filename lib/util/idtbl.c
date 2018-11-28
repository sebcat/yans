#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <lib/util/idtbl.h>

/* calculate the size of an idtbl_table of a certain capacity */
#define IDTBL_SIZE(cap_) \
    (sizeof(struct idtbl_table) + (cap_ * sizeof(struct idtbl_entry)))

/* convert an entry to a table entry index */
#define ENTRY2INDEX(tbl, ent) \
    ((uint32_t)(((ent) - (tbl)->entries)))

/* adjust 'val' to be a valid index into the 'tbl'-table */
#define TABLE_INDEX(tbl, val) \
    ((val) & (tbl)->modmask)


/* FNV1a constants */
#define FNV1A_OFFSET 0x811c9dc5
#define FNV1A_PRIME   0x1000193

 /* macros to make sure we rehash at 83.34% load, or 5/6-th of the total
  * size */
#define REHASH_ROUND_UP(val) ((val) + (val)/6) /* for allocating */
#define REHASH_LIMIT(val) ((val) - (val)/6)    /* for calculating limit */

static uint32_t hashfunc(uint32_t key, uint32_t seed) {
  uint32_t hash = seed;
  int i;

  for (i = 0; i < 4; i++) {
    hash = (hash ^ (key & 0xff)) * FNV1A_PRIME;
    key >>= 8;
  }

  return hash;
}

/* from bit-twiddling hacks, can be made more efficient with inline asm
 * and "count leading zeroes" instruction. */
static inline uint32_t round_up_pow2(uint32_t v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}

/* -1 on failed allocation, 0 on success */
static struct idtbl_table *create_table(uint32_t nslots, uint32_t seed) {
  struct idtbl_table *tbl;
  uint32_t cap;

  cap = round_up_pow2(cap); /* we're using '&' for modulus of hash */
  if (cap < nslots) {       /* check for overflow */
    return NULL;
  }

  tbl = calloc(1, IDTBL_SIZE(cap));
  if (tbl == NULL) {
    return NULL;
  }

  tbl->cap = cap;
  tbl->hashseed = hashfunc(seed, FNV1A_OFFSET);
  tbl->modmask = tbl->cap - 1; /* NB: cap is an even power of two */
  return tbl;
}

static void free_table(struct idtbl_table *tbl) {
  free(tbl);
}

static struct idtbl_entry *find_table_entry(struct idtbl_table *tbl,
    uint32_t key) {
  uint32_t current_pos;
  uint32_t start_pos;
  uint32_t distance = 0;
  struct idtbl_entry *elem;

  /* Probe linearly for at most the entire table (theoretical upper bound).
   * If the current element has a key associated with the element we want,
   * then return that value. If we find an empty entry or an entry
   * with a distance lower than our current distance we stop looking for
   * the entry and return NULL. Entries with lower distances would have
   * been replaced on insertion. */
  key++; /* internally, key 0 is used to denote an empty slot */
  current_pos = start_pos = TABLE_INDEX(tbl, hashfunc(key, tbl->hashseed));
  do {
    elem = &tbl->entries[current_pos];
    if (elem->key == key) {
      return elem;
    } else if (elem->key == 0 || elem->distance < distance) {
      break;
    }
    current_pos = TABLE_INDEX(tbl, current_pos + 1);
  } while(current_pos != start_pos);

  return NULL;
}

static int get_table_value(struct idtbl_table *tbl, uint32_t key,
    void **out) {
  struct idtbl_entry *ent;

  ent = find_table_entry(tbl, key);
  if (!ent) {
    return IDTBL_ENOTFOUND;
  }

  if (out) {
    *out = ent->value;
  }

  return IDTBL_OK;
}

static int table_contains(struct idtbl_table *tbl, uint32_t key) {
  return find_table_entry(tbl, key) == NULL ? 0 : 1;
}

static int remove_table_entry(struct idtbl_table *tbl, uint32_t key) {
  struct idtbl_entry *ent;
  uint32_t curr;
  uint32_t next;
  struct idtbl_entry empty = {0};

  ent = find_table_entry(tbl, key);
  if (!ent) {
    return IDTBL_ENOTFOUND;
  }

  curr = ENTRY2INDEX(tbl, ent);
  next = TABLE_INDEX(tbl, curr + 1);

  while (tbl->entries[next].key != 0 && tbl->entries[next].distance > 0) {
    tbl->entries[curr] = tbl->entries[next];
    tbl->entries[curr].distance--;
    curr = TABLE_INDEX(tbl, curr + 1);
    next = TABLE_INDEX(tbl, next + 1);
  }

  tbl->entries[curr] = empty;
  tbl->size--;
  return IDTBL_OK;
}

/* -1 on failed insertion (e.g., because table is full), 0 on success */
static int set_table_value(struct idtbl_table *tbl, uint32_t key,
    void *value) {
  uint32_t start_pos;
  uint32_t current_pos;
  struct idtbl_entry *curr;
  struct idtbl_entry elem;
  struct idtbl_entry tmp;

  if (key == UINT32_MAX-1) {
    return IDTBL_EINVAL; /* invalid key */
  } else if (tbl->size >= tbl->cap) {
    return IDTBL_EFULL; /* full table */
  }
  
  key++; /* internally, key 0 is used to denote an empty slot */
  current_pos = start_pos = TABLE_INDEX(tbl, hashfunc(key, tbl->hashseed));

  /* initialize our element to insert */
  elem.key = key;
  elem.value = value;
  elem.distance = 0;

  do {
    curr = &tbl->entries[current_pos];
    if (curr->key == 0) {
      /* the current slot is empty - increment the variable holding the
       * number of entries in use and insert at this position */
      tbl->size++;
      *curr = elem;
      return IDTBL_OK;
    } else if (curr->key == key) {
      /* the current slot has the same key - insert at this position,
       * overwriting the existing entry */
      *curr = elem;
      return IDTBL_OK;
    } else if (curr->distance < elem.distance) {
      /* the distance of the current entry is less than that of the
       * to-be-inserted entry - swap 'em */
      tmp = *curr;
      *curr = elem;
      elem = tmp;
    }

    elem.distance++;
    current_pos = TABLE_INDEX(tbl, current_pos + 1);
  } while(current_pos != start_pos);

  /* shouldn't happen: unless the table is full (which is
   * checked for when calling the function) - insertion of a valid
   * key/value-pair should always be possible */
  return IDTBL_ENOSLOT;
}

static int grow(struct idtbl_ctx *ctx) {
  uint32_t new_cap;
  struct idtbl_table *new_tbl;
  size_t i;
  struct idtbl_entry *ent;

  /* calculate the capacity of the new table */
  new_cap = ctx->tbl->cap << 1;
  if (new_cap < ctx->tbl->cap) {
    return IDTBL_ENOMEM;
  }

  /* allocate a new table */
  new_tbl = create_table(new_cap, ctx->tbl->hashseed);
  if (new_tbl == NULL) {
    return IDTBL_ENOMEM;
  }

  /* insert non-empty entries from the old table to the new one */
  for (i = 0; i < ctx->tbl->cap; i++) {
    ent = ctx->tbl->entries + i;
    if (ent->key != 0) {
      set_table_value(new_tbl, ent->key, ent->value);
    }
  }

  ctx->rehash_limit = REHASH_LIMIT(new_tbl->cap);
  ctx->tbl = new_tbl;
  return IDTBL_OK;
}

int idtbl_init(struct idtbl_ctx *ctx, uint32_t nslots, uint32_t seed) {
  struct idtbl_table *tbl;
  uint32_t cap;

  if (nslots == 0) {
    return IDTBL_EINVAL;
  }

  cap = REHASH_ROUND_UP(nslots);
  if (cap < nslots) {
    return IDTBL_ENOMEM;
  }

  tbl = create_table(cap, seed);
  if (tbl == NULL) {
    return IDTBL_ENOMEM;
  }

  ctx->rehash_limit = REHASH_LIMIT(tbl->cap);
  ctx->tbl = tbl;
  return IDTBL_OK;
}

void idtbl_cleanup(struct idtbl_ctx *ctx) {
  free_table(ctx->tbl);
}

int idtbl_get(struct idtbl_ctx *ctx, uint32_t key, void **value) {
  return get_table_value(ctx->tbl, key, value);
}

int idtbl_contains(struct idtbl_ctx *ctx, uint32_t key) {
  return table_contains(ctx->tbl, key);
}

int idtbl_remove(struct idtbl_ctx *ctx, uint32_t key) {
  return remove_table_entry(ctx->tbl, key);
}

int idtbl_set(struct idtbl_ctx *ctx, uint32_t key, void *value) {
  int ret;

  if (ctx->tbl->size >= ctx->rehash_limit) {
    ret = grow(ctx);
    if (ret != IDTBL_OK) {
      return ret;
    }
  }

  return set_table_value(ctx->tbl, key, value);
}

const char *idtbl_strerror(int code) {
  switch (code) {
    case IDTBL_OK:
      return "success";
    case IDTBL_ENOTFOUND:
      return "element not found";
    case IDTBL_EFULL:
      return "table full";
    case IDTBL_EINVAL:
      return "invalid argument";
    case IDTBL_ENOSLOT:
      return "no slot available";
    case IDTBL_ENOMEM:
      return "out of memory";
    default:
      return "unknown error";
  }
}
