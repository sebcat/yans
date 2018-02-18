/* vim: set tabstop=2 shiftwidth=2 expandtab ai: */
#define _GNU_SOURCE /* for getaddrinfo and s6_addr32 on linux */
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#if defined(__FreeBSD__)
#define s6_addr8  __u6_addr.__u6_addr8
#define s6_addr16 __u6_addr.__u6_addr16
#define s6_addr32 __u6_addr.__u6_addr32
#endif

#include <lib/net/ip.h>


int ip_addr(ip_addr_t *addr, const char *s, int *err) {
  struct addrinfo hints = {0};
  struct addrinfo *curr, *ai;
  char addrbuf[YANS_IP_ADDR_MAXLEN], *start, *end;
  int ret;

  assert(addr != NULL);
  assert(s != NULL);

  snprintf(addrbuf, sizeof(addrbuf), "%s", s);

  /* trim leading whitespace and end the string at the first whitespace
   * thereafter, if any */
  start = addrbuf + strspn(addrbuf, " \t\r\n");
  if (*start != '\0') {
    end = start + strcspn(start, " \t\r\n");
    *end = '\0';
  }

  hints.ai_flags = AI_NUMERICHOST|AI_NUMERICSERV;
  if ((ret = getaddrinfo(start, NULL, &hints, &ai)) != 0) {
    if (err != NULL) {
      *err = ret;
    }
    return -1;
  }

  for (curr = ai; curr != NULL; curr = curr->ai_next) {
    if (curr->ai_family == AF_INET || curr->ai_family == AF_INET6) {
      memcpy(&addr->u.ss, curr->ai_addr, curr->ai_addrlen);
      freeaddrinfo(ai);
      return 0;
    }
  }

  freeaddrinfo(ai);
  return -1;
}

int ip_addr_str(ip_addr_t *addr, char *dst, size_t dstlen, int *err) {
  int ret;

  assert(addr != NULL);
  assert(dst != NULL);

  if (addr->u.sa.sa_family == AF_INET) {
    ret = getnameinfo(&addr->u.sa, sizeof(struct sockaddr_in), dst, dstlen,
        NULL, 0, NI_NUMERICHOST);
  } else if (addr->u.sa.sa_family == AF_INET6) {
    ret = getnameinfo(&addr->u.sa, sizeof(struct sockaddr_in6), dst, dstlen,
        NULL, 0, NI_NUMERICHOST);
  } else {
    if (err != NULL) *err = EAI_FAMILY;
    return -1;
  }

  if (ret != 0) {
    if (err != NULL) *err = ret;
    return -1;
  } else {
    return 0;
  }
}

int ip_addr_cmp(const ip_addr_t *a1, const ip_addr_t *a2, int *err) {

  assert(a1 != NULL);
  assert(a2 != NULL);
  int ret;

  /* consider IPv4 addrs to be less than IPv6 */
  if (a1->u.sa.sa_family == AF_INET &&
      a2->u.sa.sa_family == AF_INET6) {
    return -1;
  } else if (a1->u.sa.sa_family == AF_INET6 &&
      a2->u.sa.sa_family == AF_INET) {
    return 1;
  }

  if (a1->u.sa.sa_family == AF_INET && a2->u.sa.sa_family == AF_INET) {
    return memcmp(&a1->u.sin.sin_addr, &a2->u.sin.sin_addr,
        sizeof(struct in_addr));
  } else if (a1->u.sa.sa_family == AF_INET6 &&
      a2->u.sa.sa_family == AF_INET6) {
    ret = memcmp(&a1->u.sin6.sin6_addr, &a2->u.sin6.sin6_addr,
        sizeof(a1->u.sin6.sin6_addr));
    /*
     * We could compare by zone-ids, but that becomes a problem in
     * compress_blocks for cases like ff02::1%em0 ff02::1%lo0 ff02::1 ff02::2,
     * which will get sorted as ff02::1 ff02::1%em0 ff02::1%lo0 ff02::2, which
     * means ff02::2 will not get compressed to the range ff02::1-ff02::2 with
     * the current way compress_blocks works. If we want
     * to compare by zone ID's, we need to change the way compare_blocks works
     *
     * if (ret == 0) {
     * ret = memcmp(&a1->u.sin6.sin6_scope_id, &a2->u.sin6.sin6_scope_id,
     *   sizeof(a1->u.sin6.sin6_scope_id));
     * }
    */
    return ret;
  } else {
    if (err != NULL) *err = EAI_FAMILY;
    return -1; /* not ideal if caller doesn't check errors */
  }
}

/* assumes 2's complement and may be more complicated than it needs to be */
static inline void ip_addr_add_ipv6(ip_addr_t *addr, int32_t n) {
  uint64_t acc = 0;
  uint32_t ext[4];
  int i;

  /* Setup ext as a 128-bit integer and sign extend if n < 0,
   * otherwise zero extend */
  ext[3] = (uint32_t)n;
  if (n < 0) {
    ext[0] = ext[1] = ext[2] = 0xffffffff;
  } else {
    ext[0] = ext[1] = ext[2] = 0;
  }

  for(i = 3; i >= 0; i--) {
    acc = ((uint64_t)ntohl(addr->u.sin6.sin6_addr.s6_addr32[i]))
        + (uint64_t)ext[i] + acc;
    addr->u.sin6.sin6_addr.s6_addr32[i] = htonl((uint32_t)acc);
    acc >>= 32;
  }
}

void ip_addr_add(ip_addr_t *addr, int32_t n) {
  assert(addr != NULL);
  if (addr->u.sa.sa_family == AF_INET) {
    addr->u.sin.sin_addr.s_addr =
        htonl(ntohl(addr->u.sin.sin_addr.s_addr) + (uint32_t)n);
  } else if (addr->u.sa.sa_family == AF_INET6) {
    ip_addr_add_ipv6(addr, n);
  }
}

void ip_addr_sub(ip_addr_t *addr, int32_t n) {
  ip_addr_add(addr, -n);
}

/* builds a netmask from a prefixlen value and stores the result in addr */
static void ip6_netmask(ip_addr_t *addr, uint32_t prefixlen) {
  uint32_t nset, nsetmod, i;
  memset(&addr->u.sin6, 0, sizeof(addr->u.sin6));
  addr->u.sin6.sin6_family = AF_INET6;

  if (prefixlen > 128) prefixlen = 128;
  nset = prefixlen/32;
  nsetmod = prefixlen%32;
  for(i=0; i<nset; i++) {
    addr->u.sin6.sin6_addr.s6_addr32[i] = 0xffffffff;
  }

  if (nsetmod != 0 && i <= 3) {
    addr->u.sin6.sin6_addr.s6_addr32[i] = htonl(~((1<<(32-nsetmod))-1));
  }
}

/* clears the lower bits of an IPv6 address based on an IPv6 netmask */
static inline void ip6_clearbits(ip_addr_t *addr, ip_addr_t *mask) {
  int i;

  for(i=0; i<4; i++) {
    addr->u.sin6.sin6_addr.s6_addr32[i] &=
        mask->u.sin6.sin6_addr.s6_addr32[i];
  }
}

/* sets the lower bits of an IPv6 address based on an IPv6 netmask */
static inline void ip6_setbits(ip_addr_t *addr, ip_addr_t *mask) {
  int i;

  for(i=0; i<4; i++) {
    addr->u.sin6.sin6_addr.s6_addr32[i] |=
        ~mask->u.sin6.sin6_addr.s6_addr32[i];
  }
}

static void ip6_block_mask(ip_addr_t *first, ip_addr_t *last,
    uint32_t prefixlen) {
  /* clear the lower bits of 'first', set the lower bits of 'last' */
  ip_addr_t mask;
  ip6_netmask(&mask, prefixlen);
  ip6_clearbits(first, &mask);
  ip6_setbits(last, &mask);
}

static void ip4_netmask(ip_addr_t *addr, uint32_t prefixlen) {
  memset(&addr->u.sin, 0, sizeof(addr->u.sin));
  addr->u.sin.sin_family = AF_INET;
  if (prefixlen == 0) {
    addr->u.sin.sin_addr.s_addr = 0;
  } else if (prefixlen > 32) {
    addr->u.sin.sin_addr.s_addr = 0xffffffff;
  } else {
    addr->u.sin.sin_addr.s_addr = htonl(~((1<<(32-prefixlen))-1));
  }
}

#define ip4_clearbits(_addr, _mask) \
    ((_addr)->u.sin.sin_addr.s_addr &= (_mask)->u.sin.sin_addr.s_addr)

#define ip4_setbits(_addr, _mask) \
    ((_addr)->u.sin.sin_addr.s_addr |= ~(_mask)->u.sin.sin_addr.s_addr)

static inline void ip4_block_mask(ip_addr_t *first, ip_addr_t *last,
    uint32_t prefixlen) {
  ip_addr_t mask;
  ip4_netmask(&mask, prefixlen);
  ip4_clearbits(first, &mask);
  ip4_setbits(last, &mask);
}

static int ip_block_cidr(ip_block_t *block, const char *addrstr,
    const char *plenstr, int *err) {
  char *cptr;
  ip_addr_t addr;
  unsigned long prefixlen;

  if (ip_addr(&addr, addrstr, err) < 0) return -1;
  prefixlen = strtoul(plenstr, &cptr, 10);
  if (*cptr != '\0') {
    if (err != NULL) *err = EAI_NONAME;
    return -1;
  }

  memcpy(&block->first, &addr, sizeof(ip_addr_t));
  memcpy(&block->last, &addr, sizeof(ip_addr_t));
  if (addr.u.sa.sa_family == AF_INET) {
    if (prefixlen > 32) {
      if (err != NULL) *err = EAI_NONAME;
      return -1;
    }

    ip4_block_mask(&block->first, &block->last, (uint32_t)prefixlen);
  } else if (addr.u.sa.sa_family == AF_INET6) {
    if (prefixlen > 128) {
      if (err != NULL) *err = EAI_NONAME;
      return -1;
    }

    ip6_block_mask(&block->first, &block->last, (uint32_t)prefixlen);
  } else {
    if (err != NULL) *err = EAI_NONAME;
    return -1;
  }

  return 0;
}

int ip_block_from_addrs(ip_block_t *blk, ip_addr_t *a1, ip_addr_t *a2,
    int *err) {
  int cmp;
  int lerr = 0;

  if (a1->u.sa.sa_family != a2->u.sa.sa_family) {
    if (err != NULL) *err = EAI_FAMILY;
    return -1;
  }

  cmp = ip_addr_cmp(a1, a2, &lerr);
  if (lerr != 0) {
    if (err != NULL) *err = lerr;
    return -1;
  }

  if (cmp < 0) {
    memcpy(&blk->first, a1, sizeof(ip_addr_t));
    memcpy(&blk->last, a2, sizeof(ip_addr_t));
  } else {
    memcpy(&blk->first, a2, sizeof(ip_addr_t));
    memcpy(&blk->last, a1, sizeof(ip_addr_t));
  }

  return 0;
}

static int ip_block_range(ip_block_t *blk, const char *first,
    const char *last, int *err) {
  ip_addr_t a1, a2;

  if (ip_addr(&a1, first, err) < 0) return -1;
  if (ip_addr(&a2, last, err) < 0) return -1;
  return ip_block_from_addrs(blk, &a1, &a2, err);
}

int ip_block_from_str(ip_block_t *blk, const char *s, int *err) {
  char *cptr;
  char addrbuf[YANS_IP_ADDR_MAXLEN];
  int ret = -1;

  snprintf(addrbuf, sizeof(addrbuf), "%s", s);
  if ((cptr = strchr(addrbuf, '/')) != NULL) {
    /* addr/prefixlen form */
    *cptr = 0;
    ret = ip_block_cidr(blk, addrbuf, cptr+1, err);
  } else if ((cptr = strchr(addrbuf, '-')) != NULL) {
    /* first-last form */
    *cptr = 0;
    ret = ip_block_range(blk, addrbuf, cptr+1, err);
  } else {
    /* assume single address form */
    if (ip_addr(&blk->first, addrbuf, err) < 0) return -1;
    memcpy(&blk->last, &blk->first, sizeof(ip_addr_t));
    ret = 0;
  }

  return ret;
}

/* naive way of calculating the prefixlen from a netmask, assuming that
 * a prefixlen is applicable (which it should be, otherwise one addr+mask can
 * turn into many blocks, and that's a PITA) */
static int calc_prefixlen(unsigned char *data, int len) {
  unsigned char *curr, buf;
  int nbits = 0;

  assert(len >= 0);
  for (curr = data + len - 1; curr >= data; curr--) {
    if (*curr == 0) {
      nbits += 8;
      continue;
    }

    buf = *curr;
    while (!(buf & 1)) {
      nbits++;
      buf = buf >> 1;
    }
    break;
  }

  nbits = len*8 - nbits;
  assert(nbits >= 0);
  return nbits;
}

int ip_block_netmask(ip_block_t *blk, ip_addr_t * addr, ip_addr_t *netmask,
    int *err) {
  int prefixlen;

  memcpy(&blk->first, addr, sizeof(ip_addr_t));
  memcpy(&blk->last, addr, sizeof(ip_addr_t));
  if (addr->u.sa.sa_family == AF_INET && netmask->u.sa.sa_family == AF_INET) {
    prefixlen = calc_prefixlen((unsigned char *)(&netmask->u.sin.sin_addr),
        sizeof(netmask->u.sin.sin_addr));
    ip4_block_mask(&blk->first, &blk->last, (uint32_t)prefixlen);
  } else if (addr->u.sa.sa_family == AF_INET6 &&
      netmask->u.sa.sa_family == AF_INET6) {
    prefixlen = calc_prefixlen((unsigned char *)(&netmask->u.sin6.sin6_addr),
        sizeof(netmask->u.sin6.sin6_addr));
    ip6_block_mask(&blk->first, &blk->last, (uint32_t)prefixlen);
  } else {
    if (err != NULL) *err = EAI_FAMILY;
    return -1;
  }
  return 0;
}

/* returns 1 if the block contains the address, 0 if not */
int ip_block_contains(ip_block_t *blk, ip_addr_t *addr) {
  if (ip_addr_eqtype(&blk->first, &blk->last) &&
      ip_addr_eqtype(&blk->first, addr) &&
      ip_addr_cmp(addr, &blk->last, NULL) <= 0 &&
      ip_addr_cmp(addr, &blk->first, NULL) >= 0) {
    return 1;
  }

  return 0;
}

int ip_block_to_str(ip_block_t *blk, char *dst, size_t dstlen, int *err) {
  char *cptr;
  size_t len;
  size_t left;

  if (ip_addr_str(&blk->first, dst, dstlen, err) < 0) {
    return -1;
  }

  if (ip_addr_cmp(&blk->first, &blk->last, err) == 0) {
    /* block consisting of only one address; we're done */
    return 0;
  }

  len = strlen(dst);
  left = dstlen - len;

  /* first-last form */
  if (left < 2) {
    if (err != NULL) *err = EAI_OVERFLOW;
    return -1;
  }
  cptr = dst+len;
  *cptr = '-';
  cptr++;
  if (ip_addr_str(&blk->last, cptr, left-1, err) < 0) {
    return -1;
  }

  return 0;
}

#define IS_IP_BLOCKS_SEP(x) \
    ((x) == '\r' ||         \
     (x) == '\n' ||         \
     (x) == '\t' ||         \
     (x) == ' '  ||         \
     (x) == ',')


static int cons_block(struct ip_blocks *blks, const char *s, size_t start,
    size_t end, int *err) {
  size_t len;
  char buf[256];
  ip_block_t blk = {{{{0}}}};
  void *tmp;

  len = end - start;
  if (len >= sizeof(buf)) {
    len = sizeof(buf) - 1;
  }
  memcpy(buf, s + start, len);
  buf[len] = '\0';
  if (ip_block_from_str(&blk, buf, err) < 0) {
    return -1;
  }

  tmp = realloc(blks->blocks, sizeof(*blks->blocks) * (blks->nblocks + 1));
  if (tmp == NULL) {
    if (err != NULL) {
      *err = EAI_MEMORY;
    }
    return -1;
  }

  blks->blocks = tmp;
  memcpy(blks->blocks + blks->nblocks, &blk, sizeof(blk));
  blks->nblocks++;
  return 0;
}

/* sorting function for use by e.g., qsort */
static int blockcmp(const void *p0, const void *p1) {
  const ip_block_t *b0 = p0;
  const ip_block_t *b1 = p1;
  int ret;

  ret = ip_addr_cmp(&b0->first, &b1->first, NULL);
  if (ret == 0) {
    return ip_addr_cmp(&b0->last, &b1->last, NULL);
  }
  return ret;
}

static size_t compress_blocks(struct ip_blocks *blks) {
  size_t curr;
  size_t next;
  ip_addr_t tmp;
  int diff_zones;

  for (curr = 0, next = 1; next < blks->nblocks; next++) {
    diff_zones = 0;
    if (blks->blocks[curr].first.u.sa.sa_family == AF_INET6 &&
        blks->blocks[next].first.u.sa.sa_family == AF_INET6 &&
        blks->blocks[curr].first.u.sin6.sin6_scope_id !=
        blks->blocks[next].first.u.sin6.sin6_scope_id) {
      diff_zones = 1;
    }

    if (!diff_zones && blks->blocks[curr].first.u.sa.sa_family ==
        blks->blocks[next].first.u.sa.sa_family) {
      tmp = blks->blocks[curr].last;
      ip_addr_add(&tmp, 1);
      if (ip_addr_cmp(&tmp, &blks->blocks[next].first, NULL) >= 0) {
        if (ip_addr_cmp(&blks->blocks[next].last, &blks->blocks[curr].last,
            NULL) > 0) {
          blks->blocks[curr].last = blks->blocks[next].last;
        }
        continue;
      }
    }

    curr++;
    if (next > curr) {
      memmove(blks->blocks + curr, blks->blocks + next,
          (blks->nblocks - next) * sizeof(ip_block_t));
      blks->nblocks -= next - curr;
      next = curr;
    }
  }

  return curr + 1;
}

int ip_blocks_init(struct ip_blocks *blks, const char *s, int *err) {
  size_t pos;
  size_t end;
  size_t addrstart;

  enum {
    BLKS_FINDSTART = 0,
    BLKS_FINDEND,
  } S = BLKS_FINDSTART;

  memset(blks, 0, sizeof(*blks));
  /* AF_UNSPEC is "always" 0, but set it explicitly for paranoia reasons */
  blks->curr_addr.u.sa.sa_family = AF_UNSPEC;
  if (s == NULL) {
    return 0;
  }

  for (pos = 0, end = strlen(s); pos < end; pos++) {
    switch(S) {
      case BLKS_FINDSTART:
        if (!IS_IP_BLOCKS_SEP(s[pos])) {
          addrstart = pos;
          S = BLKS_FINDEND;
        }
        break;
      case BLKS_FINDEND:
        if (IS_IP_BLOCKS_SEP(s[pos])) {
          if (cons_block(blks, s, addrstart, pos, err) < 0) {
            return -1;
          }
          S = BLKS_FINDSTART;
        }
        break;
    }
  }

  /* check last element, if any */
  if (S == BLKS_FINDEND) {
    if (cons_block(blks, s, addrstart, pos, err) < 0) {
      return -1;
    }
  }

  /* return if empty */
  if (blks->nblocks == 0) {
    return 0;
  }

  qsort(blks->blocks, blks->nblocks, sizeof(ip_block_t), blockcmp);
  blks->nblocks = compress_blocks(blks);
  return 0;
}

void ip_blocks_cleanup(struct ip_blocks *blks) {
  if (blks) {
    if (blks->blocks) {
      free(blks->blocks);
    }
    memset(blks, 0, sizeof(*blks));
    blks->curr_addr.u.sa.sa_family = AF_UNSPEC;
  }
}

int ip_blocks_to_buf(struct ip_blocks *blks, buf_t *buf, int *err) {
  size_t i;
  char ipbuf[128];
  ip_block_t *blk;
  ip_addr_t swp;
  int ret;

  buf_clear(buf);
  if (blks == NULL || blks->curr_block >= blks->nblocks) {
    goto success;
  }

  blk = blks->blocks + blks->curr_block;
  if (blks->curr_block == 0 && blks->curr_addr.u.sa.sa_family == AF_UNSPEC) {
    /* first entry - initialize blks->curr_addr */
    blks->curr_addr = blk->first;
  }

  swp = blk->first;
  blk->first = blks->curr_addr;
  ret = ip_block_to_str(blk, ipbuf, sizeof(ipbuf), err);
  blk->first = swp;
  if (ret < 0) {
    return -1;
  }

  if (buf_adata(buf, ipbuf, strlen(ipbuf)) < 0) {
    goto memfail;
  }

  for(i = blks->curr_block + 1; i < blks->nblocks; i++) {
    if (ip_block_to_str(&blks->blocks[i], ipbuf, sizeof(ipbuf), err) < 0) {
      return -1;
    }

    buf_achar(buf, ' ');
    if (buf_adata(buf, ipbuf, strlen(ipbuf)) < 0) {
      goto memfail;
    }
  }

success:
  /* NB: no trailing null byte added, the caller is expected to do that if
   *     the caller wants it */
  return 0;

memfail:
  if (err != NULL) {
    *err = EAI_MEMORY;
  }
  return -1;
}

int ip_blocks_next(struct ip_blocks *blks, ip_addr_t *addr) {
  ip_block_t *blk;

  /* check if we've reached the end of all blocks - reset iter and return 0 */
  if (blks->curr_block >= blks->nblocks) {
    ip_blocks_reset(blks);
    return 0;
  }

  blk = blks->blocks + blks->curr_block;
  if (blks->curr_block == 0 && blks->curr_addr.u.sa.sa_family == AF_UNSPEC) {
    /* first entry - initialize blks->curr_addr */
    blks->curr_addr = blk->first;
  }

  *addr = blks->curr_addr;
  if (ip_addr_cmp(&blks->curr_addr, &blk->last, NULL) < 0) {
    ip_addr_add(&blks->curr_addr, 1);
  } else {
    blks->curr_block++;
    if (blks->curr_block < blks->nblocks) {
      blk = blks->blocks + blks->curr_block;
      blks->curr_addr = blk->first;
    }
  }
  return 1;
}

int ip_blocks_contains(struct ip_blocks *blks, ip_addr_t *addr) {
  size_t i;
  int ret;

  for (i = blks->curr_block; i < blks->nblocks; i++) {
    ret = ip_block_contains(&blks->blocks[i], addr);
    if (ret != 0) {
      return ret;
    }
  }

  return 0;
}

const char *ip_addr_strerror(int code) {
  return gai_strerror(code);
}

const char *ip_block_strerror(int code) {
  return gai_strerror(code);
}

const char *ip_blocks_strerror(int code) {
  return gai_strerror(code);
}

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

void ip_r4block_init(struct ip_r4block *blk, uint32_t first, uint32_t last) {
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

int ip_r4block_next(struct ip_r4block *blk, uint32_t *out) {
  uint32_t ival;
  /* So, some explanation is in order. We're using an LCG
   * (linear congruential generator) with constant parameters to
   * deterministically reorder a range.
   *
   *   x_next = a * x_curr + c (mod m)
   *
   * The LCG with the constant parameters fullfills the Hull-Dobell theorem:
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
   if (blk->ival > blk->nitems || blk->flags & IP_R4BLK_OVERFLOWED) {
     ip_r4block_init(blk, blk->first, blk->last);
     return 0;
   }

   do {
     blk->curr = (blk->curr * 5 + 1) & blk->mask;
   } while (blk->curr > blk->nitems);

   *out = blk->first + blk->curr;
   ival = blk->ival + 1;
   if (ival < blk->ival) {
     blk->flags &= IP_R4BLK_OVERFLOWED;
   }
   blk->ival = ival;
   return 1;
}

static int ip_r4blocks_reset(struct ip_r4blocks *blks) {
  struct ip_block_t *currblk;
  struct ip_r4block mapgen;
  size_t nblocks;
  size_t i;
  uint32_t mapval;
  int af;

  nblocks = blks->blocks->nblocks;
  /* setup blockmap */
  ip_r4block_init(&mapgen, 0, nblocks - 1);
  for (i = 0; i < nblocks; i++) {
    ip_r4block_next(&mapgen, &mapval);
    blks->blockmap[i] = (int)mapval;
  }

  /* setup blocks and curraddrs */
  for (i = 0; i < nblocks; i++) {
    currblk = &blks->blocks->blocks[i];
    af = currblk->first.u.sa.sa_family;
    if (af == AF_INET) {
      ip_r4block_init(&blks->ip4blocks[i],
          ntohl(currblk->first.u.sin.sin_addr.s_addr),
          ntohl(currblk->last.u.sin.sin_addr.s_addr));
    } else if (af == AF_INET6) {
      blks->ip6_curraddrs[i] = currblk->first;
    } else {
      /* unsupported AF */
      return -1;
    }
  }

  return 0;
}

int ip_r4blocks_init(struct ip_r4blocks *r4, struct ip_blocks *blks) {
  int *blockmap;
  struct ip_r4block *ip4blocks;
  ip_addr_t *ip6_curraddrs;
  int ret;

  /* we use negative indices for depleted ranges, so we only support
   * INT_MAX ranges */
  if (blks->nblocks > INT_MAX) {
    return -1;
  }

  r4->blocks = blks;

  /* invariant: empty range */
  if (blks->nblocks == 0) {
    return 0;
  }

  blockmap = calloc(1, sizeof(*blockmap) * blks->nblocks);
  if (blockmap == NULL) {
    goto fail;
  }

  ip4blocks = calloc(1, sizeof(*ip4blocks) * blks->nblocks);
  if (ip4blocks == NULL) {
    goto cleanup_blockmap;
  }

  ip6_curraddrs = calloc(1, sizeof(*ip6_curraddrs) * blks->nblocks);
  if (ip6_curraddrs == NULL) {
    goto cleanup_ip4blocks;
  }

  r4->blockmap = blockmap;
  r4->ip4blocks = ip4blocks;
  r4->ip6_curraddrs = ip6_curraddrs;
  r4->mapindex = 0;
  ret = ip_r4blocks_reset(r4);
  if (ret < 0) {
    goto cleanup_ip6_curraddrs;
  }

  return 0;

cleanup_ip6_curraddrs:
  free(ip6_curraddrs);
cleanup_ip4blocks:
  free(ip4blocks);
cleanup_blockmap:
  free(blockmap);
fail:
  return -1;
}

void ip_r4blocks_cleanup(struct ip_r4blocks *blks) {
  if (blks != NULL) {
    if (blks->ip6_curraddrs) {
      free(blks->ip6_curraddrs);
    }
    if (blks->ip4blocks) {
      free(blks->ip4blocks);
    }
    if (blks->blockmap) {
      free(blks->blockmap);
    }
    /* blks->blocks is not owned by ip_r4blocks, so not free'd here */
  }
}

int ip_r4blocks_next(struct ip_r4blocks *blks, ip_addr_t *addr) {
  size_t nblocks;
  size_t curr;
  size_t mapped_index;
  struct ip_block_t *blk;
  struct ip_addr_t *curraddr;
  int af;
  int ret;
  uint32_t next4;

  nblocks = blks->blocks->nblocks;
  if (nblocks == 0) {
    return 0;
  }

  /* get the next block index */
again:
  curr = blks->mapindex;
  do {
    curr = (curr + 1) % nblocks;
  } while(curr != blks->mapindex && blks->blockmap[curr] < 0);

  /* check if iterator is depleted */
  if (curr == blks->mapindex && blks->blockmap[curr] < 0) {
    ip_r4blocks_reset(blks);
    return 0;
  }

  /* update mapindex with the new index and map the current block */
  blks->mapindex = curr;
  mapped_index = blks->blockmap[curr];
  blk = &blks->blocks->blocks[mapped_index];

  af = blk->first.u.sa.sa_family;
  if (af == AF_INET) {
    ret = ip_r4block_next(&blks->ip4blocks[mapped_index], &next4);
    if (ret == 0) {
      /* mark block as depleted and try again */
      blks->blockmap[curr] = -1;
      goto again;
    }

    /* copy the generated address to the result */
    addr->u.sa.sa_family = AF_INET;
    addr->u.sin.sin_port = 0;
    addr->u.sin.sin_addr.s_addr = htonl(next4);
  } else if (af == AF_INET6) {
    curraddr = &blks->ip6_curraddrs[mapped_index];
    *addr = *curraddr;
    if (ip_addr_cmp(curraddr, &blk->last, NULL) < 0) {
      ip_addr_add(curraddr, 1);
    } else {
      blks->blockmap[curr] = -1;
    }
  }

  return 1;
}
