#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <lib/util/objtbl.h>

/* calculate the size of an objtbl_table of a certain capacity */
#define OBJTBL_SIZE(cap_) \
    (cap_ * sizeof(struct objtbl_entry))

/* convert an entry to a table entry index */
#define ENTRY2INDEX(tbl, ent) \
    ((uint32_t)(((ent) - (tbl)->entries)))

/* adjust 'val' to be a valid index into the 'tbl'-table */
#define TABLE_INDEX(tbl, val) \
    ((val) & (tbl)->header.modmask)

 /* macros to make sure we rehash at 83.34% load, or 5/6-th of the total
  * size */
#define REHASH_ROUND_UP(val) ((val) + (val)/6) /* for allocating */
#define REHASH_LIMIT(val) ((val) - (val)/6)    /* for calculating limit */

/* an objtbl_entry is considered empty when its hash is zero. If a  hash
 * gets computed to zero, it is incremented to one. This skews the
 * distribution a little */
#define ENTRY_IS_EMPTY(e__) ((e__)->hash == 0)


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

static int init_header(struct objtbl_header *hdr, uint32_t nslots) {
  uint32_t cap;

  cap = round_up_pow2(nslots); /* we're using '&' for modulus of hash */
  if (cap < nslots) {          /* check for overflow */
    return -1;
  }

  hdr->cap = cap;
  hdr->size = 0;
  hdr->modmask = hdr->cap - 1; /* NB: cap is an even power of two */
  hdr->rehash_limit = REHASH_LIMIT(cap);
  return 0;
}

/* -1 on failed allocation, 0 on success */
static int init_table(struct objtbl_ctx *tbl, struct objtbl_opts *opts,
    uint32_t nslots) {
  int ret;
  struct objtbl_header hdr;
  struct objtbl_entry *entries;

  ret = init_header(&hdr, nslots);
  if (ret < 0) {
    return -1;
  }

  entries = calloc(1, OBJTBL_SIZE(hdr.cap));
  if (entries == NULL) {
    return -1;
  }

  tbl->header      = hdr;
  tbl->header.opts = *opts;
  tbl->entries     = entries;
  return 0;
}

static int copy_table(struct objtbl_ctx *dst,
    struct objtbl_ctx *src) {
  struct objtbl_entry *entries;

  entries = malloc(OBJTBL_SIZE(src->header.cap));
  if (entries == NULL) {
    return -1;
  }

  memcpy(entries, src->entries, OBJTBL_SIZE(src->header.cap));
  *dst = *src;
  dst->entries = entries;
  return 0;
}

void objtbl_cleanup(struct objtbl_ctx *tbl) {
  if (tbl) {
    free(tbl->entries);
    memset(tbl, 0, sizeof(*tbl));
  }
}

static objtbl_hash_t calchash(struct objtbl_ctx *tbl, const void * keyobj) {
  objtbl_hash_t h;

  h = tbl->header.opts.hashfunc(keyobj, tbl->header.opts.hashseed);
  if (h == 0) { /* 0 is used to represent an empty entry */
    h++;
  }

  return h;
}

static struct objtbl_entry *find_table_entry(struct objtbl_ctx *tbl,
    const void *keyobj) {
  uint32_t current_pos;
  uint32_t start_pos;
  uint32_t distance = 0;
  struct objtbl_entry *elem;
  objtbl_hash_t keyhash;

  /* can't find things that does not exist */
  if (keyobj == NULL) {
    return NULL;
  }

  /* Probe linearly for at most the entire table (theoretical upper bound).
   * If the current element has a key associated with the element we want,
   * then return that value. If we find an empty entry or an entry
   * with a distance lower than our current distance we stop looking for
   * the entry and return NULL. Entries with lower distances would have
   * been replaced on insertion. */
  keyhash = calchash(tbl, keyobj);
  start_pos = TABLE_INDEX(tbl, keyhash);
  current_pos = start_pos;
  do {
    elem = &tbl->entries[current_pos];
    if (ENTRY_IS_EMPTY(elem) || elem->distance < distance) {
      break;
    } else if (tbl->header.opts.cmpfunc(keyobj, elem->value) == 0) {
      return elem;
    }
    current_pos = TABLE_INDEX(tbl, current_pos + 1);
  } while(current_pos != start_pos);

  return NULL;
}

int objtbl_get(struct objtbl_ctx *tbl, const void * keyobj, void **value) {
  struct objtbl_entry *ent;

  ent = find_table_entry(tbl, keyobj);
  if (!ent) {
    return OBJTBL_ENOTFOUND;
  }

  if (value) {
    *value = ent->value;
  }

  return OBJTBL_OK;
}

int objtbl_contains(struct objtbl_ctx *tbl, void *keyobj) {
  struct objtbl_entry *ent;

  ent = find_table_entry(tbl, keyobj);
  return ent == NULL ? 0 : 1;
}

int objtbl_remove(struct objtbl_ctx *tbl, void * keyobj) {
  struct objtbl_entry *ent;
  uint32_t curr;
  uint32_t next;
  struct objtbl_entry empty = {0};

  ent = find_table_entry(tbl, keyobj);
  if (!ent) {
    return OBJTBL_ENOTFOUND;
  }

  curr = ENTRY2INDEX(tbl, ent);
  next = TABLE_INDEX(tbl, curr + 1);

  while (!ENTRY_IS_EMPTY(&tbl->entries[next]) &&
      tbl->entries[next].distance > 0) {
    tbl->entries[curr] = tbl->entries[next];
    tbl->entries[curr].distance--;
    curr = TABLE_INDEX(tbl, curr + 1);
    next = TABLE_INDEX(tbl, next + 1);
  }

  tbl->entries[curr] = empty;
  tbl->header.size--;
  return OBJTBL_OK;
}

static int set_table_value(struct objtbl_ctx *tbl, void *obj) {
  uint32_t start_pos;
  uint32_t current_pos;
  struct objtbl_entry *curr;
  struct objtbl_entry elem;
  struct objtbl_entry tmp;
  objtbl_hash_t hash;

  if (obj == NULL) {
    return OBJTBL_EINVAL;
  }

  if (tbl->header.size >= tbl->header.cap) {
    return OBJTBL_EFULL; /* full table */
  }

  hash = calchash(tbl, obj);
  start_pos = TABLE_INDEX(tbl, hash);
  current_pos = start_pos;

  /* initialize our element to insert */
  elem.hash = hash;
  elem.value = obj;
  elem.distance = 0;

  do {
    curr = &tbl->entries[current_pos];
    if (ENTRY_IS_EMPTY(curr)) {
      /* the current slot is empty - increment the variable holding the
       * number of entries in use and insert at this position */
      tbl->header.size++;
      *curr = elem;
      return OBJTBL_OK;
    } else if (tbl->header.opts.cmpfunc(obj, curr->value) == 0) {
      /* the current slot has the same key - insert at this position,
       * overwriting the existing entry */
      *curr = elem;
      return OBJTBL_OK;
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
  return OBJTBL_ENOSLOT;
}

static int grow(struct objtbl_ctx *tbl) {
  uint32_t new_cap;
  struct objtbl_ctx new_tbl;
  size_t i;
  struct objtbl_entry *ent;
  int ret;

  /* calculate the capacity of the new table */
  new_cap = tbl->header.cap << 1;
  if (new_cap < tbl->header.cap) {
    return OBJTBL_ENOMEM;
  }

  /* initialize the new table */
  ret = init_table(&new_tbl, &tbl->header.opts, new_cap);
  if (ret < 0) {
    return OBJTBL_ENOMEM;
  }

  /* insert non-empty entries from the old table to the new one */
  for (i = 0; i < tbl->header.cap; i++) {
    ent = tbl->entries + i;
    if (!ENTRY_IS_EMPTY(ent)) {
      set_table_value(&new_tbl, ent->value);
    }
  }

  /* cleanup the old table and copy the new to it */
  objtbl_cleanup(tbl);
  *tbl = new_tbl;

  return OBJTBL_OK;
}

int objtbl_init(struct objtbl_ctx *tbl, struct objtbl_opts *opts,
    uint32_t nslots) {
  uint32_t cap;
  int ret;

  if (nslots == 0) {
    nslots = 1;
  }

  cap = REHASH_ROUND_UP(nslots);
  if (cap < nslots) {
    return OBJTBL_ENOMEM;
  }

  ret = init_table(tbl, opts, cap);
  if (ret < 0) {
    return OBJTBL_ENOMEM;
  }

  return OBJTBL_OK;
}

int objtbl_insert(struct objtbl_ctx *tbl, void * obj) {
  int ret;

  if (tbl->header.size >= tbl->header.rehash_limit) {
    ret = grow(tbl);
    if (ret != OBJTBL_OK) {
      return ret;
    }
  }

  return set_table_value(tbl, obj);
}

int objtbl_copy(struct objtbl_ctx *dst, struct objtbl_ctx *src) {
  int ret;

  ret = copy_table(dst, src);
  if (ret < 0) {
    return OBJTBL_ENOMEM;
  }

  return OBJTBL_OK;
}

#ifdef __FreeBSD__
static int keycmp(void *table, const void *a, const void *b) {
  const struct objtbl_entry *ent0 = a;
  const struct objtbl_entry *ent1 = b;
  struct objtbl_ctx *tbl = table;

  if (ENTRY_IS_EMPTY(ent0) && ENTRY_IS_EMPTY(ent1)) {
    return 0;
  } else if (ENTRY_IS_EMPTY(ent0) && !ENTRY_IS_EMPTY(ent1)) {
    return 1;
  } else if (!ENTRY_IS_EMPTY(ent0) && ENTRY_IS_EMPTY(ent1)) {
    return -1;
  } else {
    return -tbl->header.opts.cmpfunc(ent0->value, ent1->value);
  }
}

int distcmp(void *tbl, const void *a, const void *b) {
  const struct objtbl_entry *ent0 = a;
  const struct objtbl_entry *ent1 = b;
  return ent1->distance - ent0->distance;
}
#else
#error "Missing qsort_r comparators (arg order varies between platforms)"
#endif

void objtbl_sort(struct objtbl_ctx *tbl) {
  qsort_r(tbl->entries, tbl->header.cap, sizeof(struct objtbl_entry), tbl,
      &keycmp);
}

int objtbl_calc_stats(struct objtbl_ctx *tbl, struct objtbl_stats *result) {
  struct objtbl_ctx dst;
  struct objtbl_entry *ent;
  size_t tot_distance = 0;
  size_t i;
  int ret;

  /* we will sort the table later, so make a copy of it */
  ret = objtbl_copy(&dst, tbl);
  if (ret != OBJTBL_OK) {
    return ret;
  }

  memset(result, 0, sizeof(*result));
  result->nbytes = sizeof(struct objtbl_ctx) +
      (sizeof(struct objtbl_entry) * dst.header.cap);
  result->size = (size_t)dst.header.size;
  result->cap = (size_t)dst.header.cap;

  /* iterate over the table summing the distances of non-empty elements */
  for (i = 0; i < dst.header.cap; i++) {
    ent = &dst.entries[i];
    if (!ENTRY_IS_EMPTY(ent)) {
      tot_distance += ent->distance;
    }
  }

  /* calculate average probe distance for all elements */
  result->average_probe_distance =
      (double)tot_distance / (double)dst.header.size;

  /* sort the table entries by keys in descending order, so we can get rid
   * of all the empty slots. Then sort them by distance, giving us
   * the mean and maximum probe values  */
  qsort_r(dst.entries, dst.header.cap, sizeof(struct objtbl_entry), &dst,
      &keycmp);
  qsort_r(dst.entries, dst.header.size, sizeof(struct objtbl_entry), &dst,
      &distcmp);
  result->max_probe_distance = dst.entries[0].distance;
  result->mean_probe_distance = dst.entries[dst.header.size >> 1].distance;
  objtbl_cleanup(&dst);
  return OBJTBL_OK;
}

void objtbl_dump(struct objtbl_ctx *ctx, FILE *fp) {
  struct objtbl_entry *ent;
  uint32_t i;

  for (i = 0; i < ctx->header.cap; i++) {
    ent = ctx->entries + i;
    fprintf(fp, "%.8u      %s distance:%u hash:%x value:%p\n",
        i, ENTRY_IS_EMPTY(ent) ? "____" : "XXXX",
        ent->distance, ent->hash, ent->value);
  }
}


const char *objtbl_strerror(int code) {
  switch (code) {
    case OBJTBL_OK:
      return "success";
    case OBJTBL_ENOTFOUND:
      return "element not found";
    case OBJTBL_EFULL:
      return "table full";
    case OBJTBL_EINVAL:
      return "invalid argument";
    case OBJTBL_ENOSLOT:
      return "no slot available";
    case OBJTBL_ENOMEM:
      return "out of memory";
    default:
      return "unknown error";
  }
}
