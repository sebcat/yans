#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <lib/util/idtbl.h>

#define IDTBL_SIZE(cap_) \
    (sizeof(struct idtbl_table) + (cap_ * sizeof(struct idtbl_entry)))

#define FNV1A_OFFSET 0x811c9dc5
#define FNV1A_PRIME   0x1000193

static uint32_t hashfunc(uint32_t key, uint32_t seed) {
  uint32_t hash = seed;
  int i;

  for (i = 0; i < 4; i++) {
    hash ^= key & 0xff;
    hash *= FNV1A_PRIME;
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
struct idtbl_table *idtbl_init(uint32_t nslots, uint32_t seed) {
  struct idtbl_table *ctx;
  uint32_t cap;

  if (nslots == 0) {
    return NULL;
  }

  /* heuristics for calculating the hash table capacity from the number
   * of required slots. */
  cap = nslots * 8 / 7;     /* lower the load factor */
  cap = round_up_pow2(cap); /* we're using '&' for modulus of hash */
  if (cap < nslots) {       /* check for overflow */
    return NULL;
  }

  ctx = calloc(1, IDTBL_SIZE(cap));
  if (ctx == NULL) {
    return NULL;
  }

  ctx->cap = cap;
  ctx->hashseed = hashfunc(seed, FNV1A_OFFSET);
  return ctx;
}

void idtbl_cleanup(struct idtbl_table *ctx) {
  free(ctx);
}

int idtbl_get(struct idtbl_table *ctx, uint32_t key, void **out) {
  uint32_t pos;
  uint32_t i;
  uint32_t modmask;
  struct idtbl_entry *curr;

  key++; /* internally, key 0 is used to denote an empty slot */
  modmask = ctx->cap - 1; /* NB: cap is an even power of two */
  pos = hashfunc(key, ctx->hashseed) & modmask;

  /* Probe linearly for at most the maximum probe length for the table.
   * If the current element has a key equal to the key of the value we
   * want - return that value. If we find an empty entry or an entry
   * with a distance lower than our current distance we stop looking for
   * the entry and return NULL. Entries with lower distances would have
   * been replaced on insertion. */
  for (i = 0; i <= ctx->max_distance; i++) {
    curr = &ctx->entries[pos];
    if (curr->key == key) {
      if (out) {
        *out = ctx->entries[pos].value;
      }
      return IDTBL_OK;
    } else if (curr->key == 0 || curr->distance < i) {
      break;
    }
    pos = (pos + 1) & modmask;
  }

  return IDTBL_ENOTFOUND;
}

/* -1 on failed insertion (e.g., because table is full), 0 on success */
int idtbl_set(struct idtbl_table *ctx, uint32_t key, void *value) {
  uint32_t start_pos;
  uint32_t current_pos;
  uint32_t modmask;
  struct idtbl_entry *curr;
  struct idtbl_entry elem;
  struct idtbl_entry tmp;

  if (key == UINT32_MAX-1) {
    return IDTBL_EINVAL; /* invalid key */
  } else if (ctx->size >= ctx->cap) {
    return IDTBL_EFULL; /* full table */
  }
  
  key++; /* internally, key 0 is used to denote an empty slot */
  modmask = ctx->cap - 1; /* NB: cap is an even power of two */
  current_pos = start_pos = hashfunc(key, ctx->hashseed) & modmask;

  /* initialize our element to insert */
  elem.key = key;
  elem.value = value;
  elem.distance = 0;

  do {
    curr = &ctx->entries[current_pos];
    if (curr->key == 0) {
      /* the current slot is empty - increment the variable holding the
       * number of entries in use and insert at this position */
      ctx->size++;
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
    if (elem.distance > ctx->max_distance) {
      ctx->max_distance = elem.distance;
    }

    current_pos = (current_pos + 1) & modmask;
  } while(current_pos != start_pos);

  /* shouldn't happen: unless the table is full (which is
   * checked for when calling the function) - insertion of a valid
   * key/value-pair should always be possible */
  return IDTBL_ENOSLOT;
}

#ifdef WOLOLO

int main(int argc, char *argv[])
{
  struct idtbl_table *tbl;
  size_t i;
  size_t sum_distance = 0;
  int initval = 2;

  if (argc == 2) {
    initval = atoi(argv[1]);
  }

  tbl = idtbl_init(initval, 0);

  for (i = 0; i < tbl->cap - tbl->cap / 8; i++) {
    idtbl_set(tbl, (i+1) * 1000, (void*)((i+1) * 4000));
  }

  for (i = 0; i < tbl->cap - tbl->cap / 8; i++) {
    size_t val = 0;
    idtbl_get(tbl, (i+1) * 1000, (void**)&val);
    fprintf(stderr, "%zu: %zu\n", i+1, val);
  }

  for (i = 0; i < tbl->cap; i++) {
    struct idtbl_entry *ent = &tbl->entries[i];
    fprintf(stderr, "index:%zu key:%u val:%u distance:%u\n", i,
        ent->key - 1, (unsigned int)ent->value, ent->distance);
    sum_distance += ent->distance;
  }

  fprintf(stderr, "max_distance:%u mean:%f\n", tbl->max_distance,
      sum_distance / (double)tbl->size);

  idtbl_cleanup(tbl);
  return 0;
}

#endif
