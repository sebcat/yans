#include <stddef.h>

#include <lib/util/reorder.h>

#define REORDER32_OVERFLOWED (1 << 0)

/* returns a mask that can be used as the next power of two modulus for
 * numbers in the range 'nitems' */
static uint32_t calc_r4block_mask(uint32_t nitems) {
  size_t i;
  uint32_t mask = 0xffffffff;

  for (i = 0; i < 8 * sizeof(nitems); i++) {
    if (nitems & (1 << ((sizeof(nitems)*8 - 1) - i))) {
      break;
    }
    mask = mask >> 1;
  }

  return mask;
}

void reorder32_init(struct reorder32 *blk, uint32_t first, uint32_t last) {
  uint32_t tmp;
  uint32_t nitems;

  /* make sure end is >= start */
  if (last < first) {
    tmp = first;
    first = last;
    last = tmp;
  }

  nitems = last - first;
  blk->flags = 0;
  blk->mask = calc_r4block_mask(nitems);
  blk->first = blk->curr = first; /* seed curr with first & mask (& in next) */
  blk->last = last;
  blk->nitems = nitems;
  blk->ival = 0;
}

int reorder32_next(struct reorder32 *blk, uint32_t *out) {
  uint32_t ival;
  /* So, some explanation is in order. We're using an LCG
   * (linear congruential generator) with constant parameters to
   * deterministically reorder a range. This can be useful when we don't want
   * to iterate over a range of e.g., ports or addresses in order but we
   * still want the reordering to be the same to avoid inconsistent results.
   *
   * An LCG looks like:
   *
   *   x_next = a * x_curr + c (mod m)
   *
   * The LCG with the constant parameters used here fullfills the Hull-Dobell
   * theorem:
   *   THEOREM 1. The sequence defined by the congruence relation (1) has full
   *   period m, provided that
   *     (i)   c is relatively prime to m;
   *     (ii)  a == 1 (mod p) if p is a prime factor of m;
   *     (iii) a == 1 (mod 4) if 4 is a factor of m
   *
   * In our case, m is the closest power of two for the range we want to
   * reorder. c is one and a is five.
   *
   * Since the period will be a power of two and our range will maybe be
   * a bit less, we reject values outside of our range. This gives us a
   * period of our range.
   *
   * Further reading: RANDOM NUMBER GENERATORS, T. E. HULL and A. R. DOBELL
   */

   /* have we reached the end and potentially overflowed? reset the iterator
    * and signal end. */
   if (blk->ival > blk->nitems || blk->flags & REORDER32_OVERFLOWED) {
     reorder32_init(blk, blk->first, blk->last);
     return 0;
   }

   do {
     blk->curr = (blk->curr * 5 + 1) & blk->mask;
   } while (blk->curr > blk->nitems);

   *out = blk->first + blk->curr;
   ival = blk->ival + 1;
   if (ival < blk->ival) {
     blk->flags &= REORDER32_OVERFLOWED;
   }
   blk->ival = ival;
   return 1;
}

