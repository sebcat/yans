#include <stdlib.h>
#include <strings.h>
#include <limits.h>
#include <string.h>

#include <lib/util/idset.h>

#define BITS_IN_BYTE 8
#define BITS_IN_INT (sizeof(int)*BITS_IN_BYTE)
#define CEIL_INTBITS(x) (((x) + BITS_IN_INT - 1u) & ~(BITS_IN_INT - 1u))
#define FLOOR_INTBITS(x) ((x) & ~(BITS_IN_INT - 1u))
#define MOD_INTBITS(x) ((x) & (BITS_IN_INT - 1u))

#define IDSETF_NONE 0
#define IDSETF_FULL (1 << 0)

/* bit states in elems:
 *   0 - used
 *   1 - cleared
 */
struct idset_ctx {
  int flags;
  int last_id;
  unsigned int nelems;
  unsigned int elems[];
};

struct idset_ctx *idset_new(unsigned int nids) {
  struct idset_ctx *ctx;
  unsigned int ceil_nids;
  unsigned int nelems;

  /* make sure nids is greater than 0 and it fits in a signed integer if we
   * round up. Check before rounding since we might wrap. */
  ceil_nids = CEIL_INTBITS(nids);
  if (nids == 0 || nids > INT_MAX || ceil_nids > INT_MAX) {
    return NULL;
  }

  nelems = ceil_nids / BITS_IN_INT;
  ctx = malloc(sizeof(struct idset_ctx) + sizeof(int)*nelems);
  if (ctx == NULL) {
    return NULL;
  }

  /* have the bitset initialized to all-ones - 1 indicates 'cleared'  */
  memset(ctx->elems, 0xff, sizeof(int) * nelems);
  ctx->nelems = nelems;
  ctx->last_id = (int)nids - 1;
  ctx->flags = IDSETF_NONE;
  return ctx;
}

void idset_free(struct idset_ctx *ctx) {
  free(ctx);
}

int idset_use_next(struct idset_ctx *ctx) {
  unsigned int i;
  unsigned int bit;
  int first_set;
  int id;

  if (ctx->flags & IDSETF_FULL) {
    return -1;
  }

  for (i = 0; i < ctx->nelems; i++) {
    first_set = ffs((int)ctx->elems[i]);
    if (first_set != 0) {
      bit = first_set - 1;
      ctx->elems[i] &= ~(1 << bit);
      id = i*BITS_IN_INT + bit;
      if (id > ctx->last_id) {
        ctx->flags |= IDSETF_FULL;
        return -1;
      }

      return id;
    }
  }

  return -1;
}

void idset_clear(struct idset_ctx *ctx, int id) {
  unsigned int elem;
  unsigned int bit;
  unsigned int uid;

  /* validate ID */
  if (id < 0 || id > ctx->last_id) {
    return;
  }

  /* calculate the affected element and bit in element */
  uid = (unsigned int)id;
  elem = FLOOR_INTBITS(uid) / BITS_IN_INT;
  bit = MOD_INTBITS(uid);

  /* if the elem is within the range, set the affected bit to 1. 
   * 1 is the 'cleared' state due to the use of ffs for finding 'cleared'
   * bits  */
  if (elem < ctx->nelems) {
    ctx->elems[elem] |= (1 << bit);
    ctx->flags &= ~IDSETF_FULL;
  }
}

