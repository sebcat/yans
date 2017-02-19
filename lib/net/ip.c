/* vim: set tabstop=2 shiftwidth=2 expandtab ai: */

#include <string.h>
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
  struct addrinfo hints;
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



  memset(&hints, 0, sizeof(hints));
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

  if (prefixlen != 0 && i <= 3) {
    addr->u.sin6.sin6_addr.s6_addr32[i] = htonl(~((1<<(32-nsetmod))-1));
  }
}

/* clears  the lower bits of an IPv6 address based on an IPv6 netmask */
static inline void ip6_clearbits(ip_addr_t *addr, ip_addr_t *mask) {
  int i;

  for(i=0; i<4; i++) {
    addr->u.sin6.sin6_addr.s6_addr32[i] &=
        mask->u.sin6.sin6_addr.s6_addr32[i];
  }
}

/* sets  the lower bits of an IPv6 address based on an IPv6 netmask */
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

static int ip_block_range(ip_block_t *block, const char *first,
    const char *last, int *err) {
  ip_addr_t a1, a2;
  int cmp, lerr = 0;

  if (ip_addr(&a1, first, err) < 0) return -1;
  if (ip_addr(&a2, last, err) < 0) return -1;
  cmp = ip_addr_cmp(&a1, &a2, &lerr);
  if (lerr != 0) {
    if (err != NULL) *err = lerr;
    return -1;
  }

  if (cmp < 0) {
    memcpy(&block->first, &a1, sizeof(ip_addr_t));
    memcpy(&block->last, &a2, sizeof(ip_addr_t));
  } else {
    memcpy(&block->first, &a2, sizeof(ip_addr_t));
    memcpy(&block->last, &a1, sizeof(ip_addr_t));
  }

  return 0;
}

int ip_block(ip_block_t *blk, const char *s, int *err) {
  char *cptr;
  char addrbuf[YANS_IP_ADDR_MAXLEN];

  snprintf(addrbuf, sizeof(addrbuf), "%s", s);
  cptr = strchr(addrbuf, '/');
  if ((cptr = strchr(addrbuf, '/')) != NULL) {
    *cptr = 0;
    return ip_block_cidr(blk, addrbuf, cptr+1, err);
  } else if ((cptr = strchr(addrbuf, '-')) != NULL) {
    *cptr = 0;
    return ip_block_range(blk, addrbuf, cptr+1, err);
  } else {
    if (err != NULL) *err = EAI_NONAME;
    return -1;
  }
}

int ip_block_netmask(ip_block_t *blk, ip_addr_t * addr, ip_addr_t *netmask,
    int *err) {
  memcpy(&blk->first, addr, sizeof(ip_addr_t));
  memcpy(&blk->last, addr, sizeof(ip_addr_t));
  if (addr->u.sa.sa_family == AF_INET && netmask->u.sa.sa_family == AF_INET) {
    ip4_clearbits(&blk->first, netmask);
    ip4_setbits(&blk->last, netmask);
    return 0;
  } else if (addr->u.sa.sa_family == AF_INET6 &&
      netmask->u.sa.sa_family == AF_INET6) {
    ip6_clearbits(&blk->first, netmask);
    ip6_setbits(&blk->last, netmask);
    return 0;
  } else {
    if (err != NULL) *err = EAI_FAMILY;
    return -1;
  }
}

/* returns 1 if the block contains the address, 0 of not, -1 on error */
int ip_block_contains(ip_block_t *blk, ip_addr_t *addr, int *err) {
  if (!ip_addr_eqtype(&blk->first, &blk->last) ||
      !ip_addr_eqtype(&blk->first, addr)) {
    if (err != NULL) *err = EAI_FAMILY;
    return -1;
  }
  if (ip_addr_cmp(addr, &blk->last, NULL) <= 0 &&
      ip_addr_cmp(addr, &blk->first, NULL) >= 0) {
    return 1;
  } else {
    return 0;
  }
}

int ip_block_str(ip_block_t *blk, char *dst, size_t dstlen, int *err) {
  char *cptr;
  size_t len, left;

  if (ip_addr_str(&blk->first, dst, dstlen, err) < 0) {
    return -1;
  }

  len = strlen(dst);
  left = dstlen - len;
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

const char *ip_addr_strerror(int code) {
  return gai_strerror(code);
}

const char *ip_block_strerror(int code) {
  return gai_strerror(code);
}