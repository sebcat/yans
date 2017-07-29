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

typedef struct ip_addr_t {
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
		struct sockaddr_storage ss;
	} u;
} ip_addr_t;

#define ip_addr_eqtype(a1, a2) ((a1)->u.sa.sa_family == (a2)->u.sa.sa_family)
int ip_addr(ip_addr_t *addr, const char *s, int *err);
int ip_addr_str(ip_addr_t *addr, char *dst, size_t dstlen, int *err);
const char *ip_addr_strerror(int code);
int ip_addr_cmp(ip_addr_t *a1, ip_addr_t *a2, int *err);
void ip_addr_add(ip_addr_t *addr, int32_t n);
void ip_addr_sub(ip_addr_t *addr, int32_t n);

typedef struct ip_block_t {
  ip_addr_t first;
  ip_addr_t last;
  int prefixlen;
} ip_block_t;

int ip_block(ip_block_t *blk, const char *s, int *err);
int ip_block_contains(ip_block_t *blk, ip_addr_t *addr);
int ip_block_str(ip_block_t *blk, char *dst, size_t dstlen, int *err);
int ip_block_netmask(ip_block_t *blk, ip_addr_t * addr, ip_addr_t *netmask,
    int *err);
const char *ip_block_strerror(int code);

struct ip_blocks {
  size_t curr;
  size_t nblocks;
  ip_block_t *blocks;
};

int ip_blocks_init(struct ip_blocks *blks, const char *s, int *err);
void ip_blocks_cleanup(struct ip_blocks *blks);
int ip_blocks_to_buf(struct ip_blocks *blks, buf_t *buf, int *err);

/* Takes the first address from blks, puts it in addr and removes it from blks.
 * Returns -1 on error, 0 when blks is empty, 1 when addr is properly set */
int ip_blocks_next(struct ip_blocks *blks, ip_addr_t *addr, int *err);

int ip_blocks_contains(struct ip_blocks *blks, ip_addr_t *addr);

const char *ip_blocks_strerror(int code);

#endif
