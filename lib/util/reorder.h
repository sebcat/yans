#ifndef YANS_REORDER_H__
#define YANS_REORDER_H__

#include <stdint.h>

/* 32-bit reordering block */
struct reorder32 {
  int flags;
  uint32_t mask;   /* mask (power of two modulus) */
  uint32_t first;  /* first in range */
  uint32_t last;   /* last in range */
  uint32_t curr;   /* current LCG value */
  uint32_t nitems; /* number of items in range */
  uint32_t ival;   /* current range iterator value */
};

void reorder32_init(struct reorder32 *blk, uint32_t start, uint32_t end);
int reorder32_next(struct reorder32 *blk, uint32_t *out);

#endif
