/* vim: set tabstop=2 shiftwidth=2 expandtab ai: */
#ifndef __YANS_IP_H
#define __YANS_IP_H

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <stdint.h>

#include <lib/util/buf.h>

#define YANS_IP_ADDR_MAXLEN 512

#define IP_R4BLK_OVERFLOWED (1 << 0)

typedef struct ip_addr_t {
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
		struct sockaddr_storage ss;
	} u;
} ip_addr_t;

typedef struct ip_block_t {
  ip_addr_t first;
  ip_addr_t last;
} ip_block_t;

struct ip_blocks {
  size_t nblocks;
  ip_block_t *blocks;
  /* iteration state */
  size_t curr_block;
  ip_addr_t curr_addr;
};

/* 32-bit reordering block */
struct ip_r4block {
  int flags;
  uint32_t mask;   /* mask (power of two modulus) */
  uint32_t first;  /* first in range */
  uint32_t last;   /* last in range */
  uint32_t curr;   /* current LCG value */
  uint32_t nitems; /* number of items in range */
  uint32_t ival;   /* current range iterator value */
};

struct ip_r4blocks {
  struct ip_blocks *blocks;
  int *blockmap;
  struct ip_r4block *ip4blocks;
  ip_addr_t *ip6_curraddrs;
  size_t mapindex;
};

#define ip_blocks_reset(blks)                     \
    (blks)->curr_block = 0;                       \
    (blks)->curr_addr.u.sa.sa_family = AF_UNSPEC;

#define ip_addr_eqtype(a1, a2) ((a1)->u.sa.sa_family == (a2)->u.sa.sa_family)
int ip_addr(ip_addr_t *addr, const char *s, int *err);
int ip_addr_str(ip_addr_t *addr, char *dst, size_t dstlen, int *err);
const char *ip_addr_strerror(int code);
int ip_addr_cmp(const ip_addr_t *a1, const ip_addr_t *a2, int *err);
void ip_addr_add(ip_addr_t *addr, int32_t n);
void ip_addr_sub(ip_addr_t *addr, int32_t n);

int ip_block_from_addrs(ip_block_t *blk, ip_addr_t *first, ip_addr_t *last,
    int *err);
int ip_block_from_str(ip_block_t *blk, const char *s, int *err);
int ip_block_to_str(ip_block_t *blk, char *dst, size_t dstlen, int *err);
int ip_block_contains(ip_block_t *blk, ip_addr_t *addr);
int ip_block_netmask(ip_block_t *blk, ip_addr_t * addr, ip_addr_t *netmask,
    int *err);
const char *ip_block_strerror(int code);

int ip_blocks_init(struct ip_blocks *blks, const char *s, int *err);
void ip_blocks_cleanup(struct ip_blocks *blks);
int ip_blocks_to_buf(struct ip_blocks *blks, buf_t *buf, int *err);

/* Takes the first address from blks, puts it in addr and prepares the next
 * one. When an iteration cycle is complete, the blocks are reset.
 * Returns -1 on error, 0 when blks is empty, 1 when addr is properly set */
int ip_blocks_next(struct ip_blocks *blks, ip_addr_t *addr);
int ip_blocks_contains(struct ip_blocks *blks, ip_addr_t *addr);
const char *ip_blocks_strerror(int code);

void ip_r4block_init(struct ip_r4block *blk, uint32_t start, uint32_t end);
int ip_r4block_next(struct ip_r4block *blk, uint32_t *out);

int ip_r4blocks_init(struct ip_r4blocks *r4, struct ip_blocks *blks);
void ip_r4blocks_cleanup(struct ip_r4blocks *blks);
int ip_r4blocks_next(struct ip_r4blocks *blks, ip_addr_t *addr);

#endif
