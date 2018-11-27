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
  struct idtbl_table *tbl;
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

  tbl = calloc(1, IDTBL_SIZE(cap));
  if (tbl == NULL) {
    return NULL;
  }

  tbl->cap = cap;
  tbl->hashseed = hashfunc(seed, FNV1A_OFFSET);
  tbl->modmask = tbl->cap - 1; /* NB: cap is an even power of two */
  return tbl;
}

void idtbl_cleanup(struct idtbl_table *tbl) {
  free(tbl);
}

static struct idtbl_entry *find_entry(struct idtbl_table *tbl,
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

int idtbl_get(struct idtbl_table *tbl, uint32_t key, void **out) {
  struct idtbl_entry *ent;

  ent = find_entry(tbl, key);
  if (!ent) {
    return IDTBL_ENOTFOUND;
  }

  if (out) {
    *out = ent->value;
  }

  return IDTBL_OK;
}

int idtbl_contains(struct idtbl_table *tbl, uint32_t key) {
  return find_entry(tbl, key) != NULL ? IDTBL_OK : IDTBL_ENOTFOUND;
}

int idtbl_remove(struct idtbl_table *tbl, uint32_t key) {
  struct idtbl_entry *ent;
  uint32_t curr;
  uint32_t next;
  struct idtbl_entry empty = {0};

  ent = find_entry(tbl, key);
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
int idtbl_set(struct idtbl_table *tbl, uint32_t key, void *value) {
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
    default:
      return "unknown error";
  }
}
