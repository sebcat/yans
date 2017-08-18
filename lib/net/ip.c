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

int ip_addr_cmp(ip_addr_t *a1, ip_addr_t *a2, int *err) {

  assert(a1 != NULL);
  assert(a2 != NULL);

  if (a1->u.sa.sa_family == AF_INET && a2->u.sa.sa_family == AF_INET) {
    return ntohl(a1->u.sin.sin_addr.s_addr)-ntohl(a2->u.sin.sin_addr.s_addr);
  } else if (a1->u.sa.sa_family == AF_INET6 &&
      a2->u.sa.sa_family == AF_INET6) {
    return memcmp(&a1->u.sin6.sin6_addr, &a2->u.sin6.sin6_addr,
        sizeof(struct in6_addr));
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

  /* NB: it's important that prefixlen is checked for large values before this
   *     cast. */
  block->prefixlen = (int)prefixlen;
  return 0;
}

int ip_block_from_addrs(ip_block_t *blk, ip_addr_t *a1, ip_addr_t *a2,
    int *err) {
  int cmp;
  int lerr = 0;

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

  blk->prefixlen = -1; /* -1 means that prefixlen is not applicable */
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
    blk->prefixlen = prefixlen;
  } else if (addr->u.sa.sa_family == AF_INET6 &&
      netmask->u.sa.sa_family == AF_INET6) {
    prefixlen = calc_prefixlen((unsigned char *)(&netmask->u.sin6.sin6_addr),
        sizeof(netmask->u.sin6.sin6_addr));
    ip6_block_mask(&blk->first, &blk->last, (uint32_t)prefixlen);
    blk->prefixlen = prefixlen;
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
  size_t numlen;
  char num[8];

  if (ip_addr_str(&blk->first, dst, dstlen, err) < 0) {
    return -1;
  }

  if (ip_addr_cmp(&blk->first, &blk->last, err) == 0) {
    /* block consisting of only one address; we're done */
    return 0;
  }

  len = strlen(dst);
  left = dstlen - len;

  if (blk->prefixlen < 0) {
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
  } else {
    /* addr/prefixlen form */
    snprintf(num, sizeof(num), "%d", blk->prefixlen);
    numlen = strlen(num);
    if (left < 2 + numlen) {
      if (err != NULL) *err = EAI_OVERFLOW;
      return -1;
    }
    cptr = dst + len;
    *cptr = '/';
    cptr++;
    snprintf(cptr, left-1, "%d", blk->prefixlen);
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

void ip_blocks_reset(struct ip_blocks *blks) {
  blks->curr_block = 0;
  blks->curr_addr.u.sa.sa_family = AF_UNSPEC;
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

  if (blk->prefixlen >= 0) {
    /* if we 'next' ::/n, n is no longer valid. store it away for now */
    blks->curr_prefixlen = blk->prefixlen;
    blk->prefixlen = -1;
  }

  *addr = blks->curr_addr;
  if (ip_addr_cmp(&blks->curr_addr, &blk->last, NULL) < 0) {
    ip_addr_add(&blks->curr_addr, 1);
  } else {
    blk->prefixlen = blks->curr_prefixlen; /* restore block prefixlen */
    blks->curr_block++;
    blk = blks->blocks + blks->curr_block;
    blks->curr_addr = blk->first;
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

uint16_t ip_csum(uint16_t init, void *data, size_t size)
{
  uint32_t inter = (uint32_t)init;
  uint16_t *curr = data;

  while(size > 1) {
    inter += *curr++;
    size -= 2;
  }

  if(size > 0) {
    inter += *(uint8_t*)curr;
  }

  inter = (inter >> 16) + (inter & 0xffff);
  inter += (inter >> 16);
  return (uint16_t)(~inter);
}
