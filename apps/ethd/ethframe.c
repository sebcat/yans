#include <string.h>
#include <net/if.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#include <lib/util/netstring.h>
#include <lib/util/flagset.h>
#include <lib/util/ylog.h>
#include <lib/util/io.h>

#include <lib/net/iface.h>

#include <apps/ethd/ethframe.h>

#if ETH_ALEN != 6
#error "inconsistent ETH_ALEN length"
#endif

/* general settings */
#define NBR_IFACES           32   /* maximum number of network interfaces */
#define YIELD_NPACKETS     1024   /* number of packets to send before yield */

/* frame_builder flags */
#define REQUIRES_IP4  (1 << 0) /* dst and src addrs must be IPv4 */
#define REQUIRES_IP6  (1 << 1) /* dst and src addrs must be IPv6 */
#define IP_DST_SWEEP  (1 << 2) /* advances the destination IP address */
#define IP_PORT_SWEEP (1 << 3) /* advances the destination port. Implies
                                  IP_DST_SWEEP */

/* ethframe_cli flags */
#define CLIFLAG_HASMSGBUF (1 << 0)

/* frame categories */
#define CAT_ARPREQ      (1 << 0)
#define CAT_LLDISCO     (1 << 1)
#define CAT_ICMPECHO    (1 << 2)
#define CAT_TCPSYN      (1 << 3)

#define ETHFRAME_CLIENT(cli__) \
  ((struct ethframe_client *)((cli__)->udata))

struct ethframe_ctx {
  int nifs;
  char ifs[NBR_IFACES][IFNAMSIZ];
  struct eth_sender senders[NBR_IFACES];
};

static struct flagset_map category_flags[] = {
  FLAGSET_ENTRY("arp-req", CAT_ARPREQ),
  FLAGSET_ENTRY("ll-disco", CAT_LLDISCO),
  FLAGSET_ENTRY("icmp-echo", CAT_ICMPECHO),
  FLAGSET_ENTRY("tcp-syn", CAT_TCPSYN),
  FLAGSET_END
};

/* process context */
static struct ethframe_ctx ethframe_ctx;

struct frame_builder {
  unsigned int category; /* what category flag(s) must be set */
  unsigned int options;  /* option flag(s) */
  const char *(*build)(struct frameconf *cfg, size_t *len);
};

static uint32_t ip_csum(uint32_t sum, void *data, size_t size) {
  uint16_t *curr = data;

  while (size > 1) {
    sum += *curr++;
    size -= 2;
  }

  if (size > 0) {
    sum += *(uint8_t*)curr;
  }

  return sum;
}

static uint16_t ip_csum_fold(uint32_t sum) {
  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  return (uint16_t)(~sum);
}

static const char *gen_custom_frame(struct frameconf *cfg, size_t *len) {
  const char *frame;

  if (cfg->curr_custom_frame >= cfg->ncustom_frames) {
    return NULL;
  }

  *len = cfg->custom_frames[cfg->curr_custom_frame].len;
  frame = cfg->custom_frames[cfg->curr_custom_frame].data;
  cfg->curr_custom_frame++;
  return frame;
}

static const char *gen_arp_req(struct frameconf *cfg, size_t *len) {
  static char pkt[] = {
    /* Ethernet */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* dst */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* src */
    0x08, 0x06, /* ethertype: ARP */

    /* ARP */
    0x00, 0x01, /* HTYPE: Ethernet */
    0x08, 0x00, /* PTYPE: IPv4 */
    0x06,       /* HLEN: 6*/
    0x04,       /* PLEN: 4*/
    0x00, 0x01, /* OPER: request */
    /* SHA (off: 22) */
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    /* SPA (off: 28) */
    0x00, 0x00,
    0x00, 0x00,
    /* THA (N/A in request) (off: 32) */
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    /* TPA: addr to look up (off: 38) */
    0x00, 0x00,
    0x00, 0x00,
  };

  memcpy(pkt + 6, cfg->eth_src, ETH_ALEN);    /* eth source */
  memcpy(pkt + 22, cfg->eth_src, ETH_ALEN);   /* ARP SHA */
  *(uint32_t*)(pkt + 28) =
      cfg->src_ip.u.sin.sin_addr.s_addr; /* ARP SPA */
  *(uint32_t*)(pkt + 38) =
      cfg->curr_dst_ip.u.sin.sin_addr.s_addr; /* ARP TPA */
  *len = sizeof(pkt);
  return pkt;
}

static const char *_icmp4_req(struct frameconf *cfg, size_t *len,
    const char *eth_dst, uint32_t ip_dst) {
  uint32_t sum;
  static char pkt[] = {
    /* Ethernet */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* dst */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* src */
    0x08, 0x00, /* ethertype: IPv4 */

    /* IPv4 (off: 14) */
    0x45, /* v: 4, 20B hdr */
    0x00,
    0x00, 0x1e, /* total length */
    0x02, 0x9a, /* ID */
    0x00, 0x00, /* flags, fragment offset */
    0x40,       /* TTL */
    0x01,       /* IPPROTO_ICMP */
    0x00, 0x00, /* checksum */
    /* src addr */
    0x00, 0x00,
    0x00, 0x00,
    /* dst addr */
    0x00, 0x00,
    0x00, 0x00,

    /* ICMPv4 (off: 34) */
    0x08,       /* type */
    0x00,       /* code */
    0x8c, 0xfc, /* checksum XXX: change if ICMP part changes */
    0x02, 0x9a, /* id */
    0x00, 0x00, /* seq */
    'h', 'i',   /* data */
  };

  /* set up the src and dst addresses */
  memcpy(pkt, eth_dst, ETH_ALEN);
  memcpy(pkt + 6, cfg->eth_src, ETH_ALEN);
  *(uint32_t*)(pkt + 26) = cfg->src_ip.u.sin.sin_addr.s_addr;
  *(uint32_t*)(pkt + 30) = ip_dst;

#if 0
  /* calculate ICMP header checksum */
  *(uint16_t*)(pkt + 36) = 0; /* clear from previous runs */
  sum = ip_csum(0, pkt + 34, 10);
  *(uint16_t*)(pkt + 36) = ip_csum_fold(sum);
#endif

  /* calculate IP header checksum */
  *(uint16_t*)(pkt + 24) = 0; /* clear from previous runs */
  sum = ip_csum(0, pkt + 14, 20);
  *(uint16_t*)(pkt + 24) = ip_csum_fold(sum);

  *len = sizeof(pkt);
  return pkt;
}

static const char *gen_icmp4_req(struct frameconf *cfg, size_t *len) {
  return _icmp4_req(cfg, len, cfg->eth_dst,
      cfg->curr_dst_ip.u.sin.sin_addr.s_addr);
}

static const char *gen_ll_icmp4_req(struct frameconf *cfg, size_t *len) {
  static const char *eth_dst = "\xff\xff\xff\xff\xff\xff";
  static const char *ip_dst = "\xff\xff\xff\xff";

  if (cfg->curr_buildstate >= 1) {
    return NULL;
  }

  return _icmp4_req(cfg, len, eth_dst, *(uint32_t*)ip_dst);
}

static const char *_icmp6_req(struct frameconf *cfg, size_t *len,
    const char *eth_dst, struct in6_addr *ip_dst) {
  uint32_t sum;
  uint32_t tmp;
  static char pkt[] = {
    /* Ethernet */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* dst */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* src */
    0x86, 0xdd, /* ethertype: IPv6 */

    /* IPv6 */
    0x60, 0x00, /* v6, TC: 0, flow lbl: 0*/
    0x00, 0x00,
    0x00, 0x0a, /* payload len: 10 */
    0x3a,       /* Next header: ICMPv6 */
    0x40,       /* hop limit */
    /* src addr */
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    /* dst addr */
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,

    /* ICMPv6 */
    0x80,       /* type: echo request */
    0x00,       /* code: 0 */
    0x00, 0x00, /* checksum */
    0x02, 0x9a, /* ID */
    0x00, 0x00, /* seq */
    'h', 'i',   /* data */
  };

  /* set up the src and dst addresses */
  memcpy(pkt, eth_dst, ETH_ALEN);
  memcpy(pkt + 6, cfg->eth_src, ETH_ALEN);
  memcpy(pkt + 22, &cfg->src_ip.u.sin6.sin6_addr, 16);
  memcpy(pkt + 38, ip_dst, 16);

  /* calculate the checksum (TODO: macro for adding len, next to sum direct) */
  *(uint16_t*)(pkt + 56) = 0; /* clear from previous runs */
  sum = ip_csum(0, pkt + 22, 32); /* pseudo-header: addrs */
  tmp = htonl(10);
  sum = ip_csum(sum, &tmp, sizeof(tmp)); /* ICMPv6 len */
  tmp = htonl(58);
  sum = ip_csum(sum, &tmp, sizeof(tmp)); /* ICMPv6 next type */
  sum = ip_csum(sum, pkt + 54, 10);
  *(uint16_t*)(pkt + 56) = ip_csum_fold(sum);

  *len = sizeof(pkt);
  return pkt;
}

static const char *gen_icmp6_req(struct frameconf *cfg, size_t *len) {
  return _icmp6_req(cfg, len, cfg->eth_dst,
      &cfg->curr_dst_ip.u.sin6.sin6_addr);
}

static const char *gen_ll_icmp6_req(struct frameconf *cfg, size_t *len) {
  static const char *eth_dst = "\x33\x33\x00\x00\x00\x01";
  static const char *ip_dst =
      "\xff\x02\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x01";

  if (cfg->curr_buildstate >= 1) {
    return NULL;
  }

  return _icmp6_req(cfg, len, eth_dst, (struct in6_addr *)ip_dst);
}

struct mdns_query {
  const char *qname;
  const uint8_t qtype[2];
};

#define QTYPE_TXT {0x00, 0x10}
#define QTYPE_SRV {0x00, 0x21}
#define QTYPE_PTR {0x00, 0x0c}

static struct mdns_query mdns_queries[] = {
  {
    .qname = "\x09_services\x07_dns-sd\x04_udp\x05local",
    .qtype = QTYPE_PTR,
  },
};

static const char *gen_mdns4_req(struct frameconf *cfg, size_t *len) {
  struct mdns_query *query;
  size_t tmplen;
  uint32_t sum;
  uint16_t tmp;
  static char pkt[] = {
    /* Ethernet */
    0x01, 0x00, 0x5E, 0x00, 0x00, 0xFB, /* dst */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* src */
    0x08, 0x00, /* ethertype: IPv4 */

    /* IPv4 */
    0x45, /* v: 4, 20B hdr */
    0x00,
    0x00, 0x3c, /* total length */
    0x02, 0x9a, /* ID */
    0x40, 0x00, /* flags (DF), fragment offset: 0 */
    0xff,       /* TTL */
    0x11,       /* IPPROTO_UDP */
    0x00, 0x00, /* checksum */
    /* src addr */
    0x00, 0x00,
    0x00, 0x00,
    /* dst addr: 224.0.0.251 */
    0xe0, 0x00,
    0x00, 0xfb,

    /* UDPv4 (off: 34) */
    0x14, 0xe9, /* src port */
    0x14, 0xe9, /* dst port */
    0x00, 0x00, /* len */
    0x00, 0x00, /* checksum */

    /* mDNS (off: 42) */
    0x00, 0x00, /* transaction ID */
    0x00, 0x00, /* flags: QUERY */
    0x00, 0x01, /* # of questions */
    0x00, 0x00, /* # of answers */
    0x00, 0x00, /* # of authority resource records */
    0x00, 0x00, /* # of additional resource records */
    /* space for QNAME, QTYPE, UNICAST-RESPONSE, QCLASS (256 + 4) (off: 54) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  };

  if (cfg->curr_buildstate >= sizeof(mdns_queries)/sizeof(struct mdns_query)) {
    return NULL;
  }

  /* set up source addresses */
  memcpy(pkt + 6, cfg->eth_src, ETH_ALEN);
  *(uint32_t*)(pkt + 26) = cfg->src_ip.u.sin.sin_addr.s_addr;

  /* set up QNAME, QTYPE, UNICAST_RESPONSE, QCLASS */
  query = &mdns_queries[cfg->curr_buildstate];
  tmplen = strlen(query->qname) + 1;
  assert(tmplen < 256);
  memcpy(pkt + 54, query->qname, tmplen);
  memcpy(pkt + 54 + tmplen, query->qtype, sizeof(query->qtype));
  *(uint16_t*)(pkt + 54 + tmplen + sizeof(query->qtype)) = htons(0x8001);

  /* set up UDP len */
  tmplen = /* UDP header*/ 8 + /*hdr*/ 12 + /*QNAME*/ tmplen +
      sizeof(query->qtype) + /*QCLASS*/ 2;
  *(uint16_t*)(pkt + 38) = htons((uint16_t)tmplen);

  /* setup IP total length */
  *(uint16_t*)(pkt + 16) = htons((uint16_t)tmplen + 20);

  /* UDP checksum */
  *(uint16_t*)(pkt + 40) = 0; /* clear from previous runs */
  sum = ip_csum(0, pkt + 26, 8); /* pseudo-header: addrs */
  tmp = htons(17); /* res, IPPROTO_UDP */
  sum = ip_csum(sum, &tmp, sizeof(tmp));
  tmp = htons((uint16_t)tmplen); /* length */
  sum = ip_csum(sum, &tmp, sizeof(tmp));
  sum = ip_csum(sum, pkt + 34, tmplen);
  *(uint16_t*)(pkt + 40) = ip_csum_fold(sum);

  /* IP checksum */
  *(uint16_t*)(pkt + 24) = 0; /* clear from previous runs */
  sum = ip_csum(0, pkt + 14, 20);
  *(uint16_t*)(pkt + 24) = ip_csum_fold(sum);

  *len = 34 + tmplen;
  return pkt;
}

static const char *gen_mdns6_req(struct frameconf *cfg, size_t *len) {
  uint32_t sum;
  uint32_t tmp;
  size_t tmplen;
  struct mdns_query *query;
  static char pkt[] = {
    /* Ethernet */
    0x33, 0x33, 0x00, 0x00, 0x00, 0xFB, /* dst */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* src */
    0x86, 0xdd, /* ethertype: IPv6 */

    /* IPv6 */
    0x60, 0x00, /* v6, TC: 0, flow lbl: 0*/
    0x00, 0x00,
    0x00, 0x00, /* payload len: 10 */
    0x11,       /* Next header: UDP */
    0xff,       /* hop limit */
    /* src addr */
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    /* dst addr FF02::FB */
    0xFF, 0x02,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0xFB,

    /* UDPv6 (off: 54) */
    0x14, 0xe9, /* src port */
    0x14, 0xe9, /* dst port */
    0x00, 0x00, /* len */
    0x00, 0x00, /* checksum */

    /* mDNS (off: 62) */
    0x00, 0x00, /* transaction ID */
    0x00, 0x00, /* flags: QUERY */
    0x00, 0x01, /* # of questions */
    0x00, 0x00, /* # of answers */
    0x00, 0x00, /* # of authority resource records */
    0x00, 0x00, /* # of additional resource records */
    /* space for QNAME, QTYPE, UNICAST-RESPONSE, QCLASS (256 + 4) (off: 74) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,

  };

  if (cfg->curr_buildstate >= sizeof(mdns_queries)/sizeof(struct mdns_query)) {
    return NULL;
  }

  /* set up src addrs */
  memcpy(pkt + 6, cfg->eth_src, ETH_ALEN);
  memcpy(pkt + 22, &cfg->src_ip.u.sin6.sin6_addr, 16);

  /* set up QNAME, QTYPE, UNICAST_RESPONSE, QCLASS */
  query = &mdns_queries[cfg->curr_buildstate];
  tmplen = strlen(query->qname) + 1;
  assert(tmplen < 256);
  memcpy(pkt + 74, query->qname, tmplen);
  memcpy(pkt + 74 + tmplen, query->qtype, sizeof(query->qtype));
  *(uint16_t*)(pkt + 74 + tmplen + sizeof(query->qtype)) = htons(0x8001);

  /* set up lengths */
  tmplen = /* UDP header*/ 8 + /*hdr*/ 12 + /*QNAME*/ tmplen +
      sizeof(query->qtype) + /*QCLASS*/ 2;
  *(uint16_t*)(pkt + 58) = htons((uint16_t)tmplen); /* UDP length */
  *(uint16_t*)(pkt + 18) = htons((uint16_t)tmplen); /* IPv6 total length */

  /* calculate and set the checksum */
  *(uint16_t*)(pkt + 60) = 0; /* clear from previous runs */
  sum = ip_csum(0, pkt + 22, 32); /* pseudo-header: addrs */
  tmp = htonl((uint32_t)tmplen);
  sum = ip_csum(sum, &tmp, sizeof(tmp)); /* TCPv6 len */
  tmp = htonl(17);
  sum = ip_csum(sum, &tmp, sizeof(tmp)); /* TCP next type */
  sum = ip_csum(sum, pkt + 54, tmplen);
  *(uint16_t*)(pkt + 60) = ip_csum_fold(sum);

  *len = 54 + tmplen;
  return pkt;
}

static const char *gen_tcp4_syn(struct frameconf *cfg, size_t *len) {
  uint32_t sum;
  uint16_t tmp;
  static char pkt[] = {
    /* Ethernet */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* dst */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* src */
    0x08, 0x00, /* ethertype: IPv4 */

    /* IPv4 */
    0x45, /* v: 4, 20B hdr */
    0x00,
    0x00, 0x3c, /* total length */
    0x02, 0x9a, /* ID */
    0x40, 0x00, /* flags (DF), fragment offset */
    0x40,       /* TTL */
    0x06,       /* IPPROTO_TCP */
    0x00, 0x00, /* checksum */
    /* src addr */
    0x00, 0x00,
    0x00, 0x00,
    /* dst addr */
    0x00, 0x00,
    0x00, 0x00,

    /* TCPv4 */
    0x7f, 0x00,  /* src port */
    0x00, 0x00,  /* dst port */
    0x02, 0x9a,  /* seq */
    0x02, 0x9a,  /* seq */
    0x00, 0x00,  /* ack */
    0x00, 0x00,  /* ack */
    0xa0, 0x02, /* header length: 40, flags: SYN*/
    0xff, 0xff,  /* window size */
    0x00, 0x00,  /* checksum */
    0x00, 0x00,  /* URG */

    /* TCPv4 options */
    0x02, 0x04,  /* kind, len: MSS, 4 */
    0x3f, 0xd8,
    0x01,        /* NOP */
    0x03, 0x03,  /* kind, len: window scale, 3 */
    0x06,
    0x04, 0x02,  /* kind, len: TCP SACK, 2 */
    0x08, 0x0a,  /* kind, len: TSOpt, 10 */
    0x00, 0x00,  /* TSval: 0 */
    0x00, 0x00,
    0x00, 0x00,  /* TSecr: 0 */
    0x00, 0x00,
  };

  /* set up the src and dst addresses */
  memcpy(pkt, cfg->eth_dst, ETH_ALEN);
  memcpy(pkt + 6, cfg->eth_src, ETH_ALEN);
  *(uint32_t*)(pkt + 26) = cfg->src_ip.u.sin.sin_addr.s_addr;
  *(uint32_t*)(pkt + 30) = cfg->curr_dst_ip.u.sin.sin_addr.s_addr;
  *(uint16_t*)(pkt + 36) = htons(cfg->curr_dst_port);

  /* TCP checksum */
  *(uint16_t*)(pkt + 50) = 0; /* clear from previous runs */
  sum = ip_csum(0, pkt + 26, 8); /* pseudo-header: addrs */
  tmp = htons(6); /* res, IPPROTO_TCP */
  sum = ip_csum(sum, &tmp, sizeof(tmp));
  tmp = htons(40); /* length */
  sum = ip_csum(sum, &tmp, sizeof(tmp));
  sum = ip_csum(sum, pkt + 34, 40);
  *(uint16_t*)(pkt + 50) = ip_csum_fold(sum);

  /* IP checksum */
  *(uint16_t*)(pkt + 24) = 0; /* clear from previous runs */
  sum = ip_csum(0, pkt + 14, 20);
  *(uint16_t*)(pkt + 24) = ip_csum_fold(sum);

  *len = sizeof(pkt);
  return pkt;
}

static const char *gen_tcp6_syn(struct frameconf *cfg, size_t *len) {
  uint32_t tmp;
  uint32_t sum;
  static char pkt[] = {
    /* Ethernet */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* dst */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* src */
    0x86, 0xdd, /* ethertype: IPv6 */

    /* IPv6 */
    0x60, 0x00, /* v6, TC: 0, flow lbl: 0*/
    0x00, 0x00,
    0x00, 0x28, /* payload len: 40 */
    0x06,       /* Next header: IPPROTO_TCP */
    0x40,       /* hop limit */
    /* src addr */
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    /* dst addr */
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,

    /* TCPv6 (off: 54) */
    0x7f, 0x00,  /* src port */
    0x00, 0x00,  /* dst port */
    0x02, 0x9a,  /* seq */
    0x02, 0x9a,  /* seq */
    0x00, 0x00,  /* ack */
    0x00, 0x00,  /* ack */
    0xa0, 0x02,  /* header length: 40B, flags: SYN */
    0xff, 0xff, /* window size */
    0x00, 0x00, /* checksum */
    0x00, 0x00, /* URG */

    /* TCPv6 options */
    0x02, 0x04,  /* kind, len: MSS, 4 */
    0x3f, 0xc4,
    0x01,        /* NOP */
    0x03, 0x03,  /* kind, len: window scale, 3 */
    0x06,
    0x04, 0x02,  /* kind, len: TCP SACK, 2 */
    0x08, 0x0a,  /* kind, len: TSOpt, 10 */
    0x00, 0x00,  /* TSval: 666 */
    0x02, 0x9a,
    0x00, 0x00,  /* TSecr: 0 */
    0x00, 0x00,
  };

  /* set up the src and dst addresses */
  memcpy(pkt, cfg->eth_dst, ETH_ALEN);
  memcpy(pkt + 6, cfg->eth_src, ETH_ALEN);
  memcpy(pkt + 22, &cfg->src_ip.u.sin6.sin6_addr, 16);
  memcpy(pkt + 38, &cfg->curr_dst_ip.u.sin6.sin6_addr, 16);
  *(uint16_t*)(pkt + 56) = htons(cfg->curr_dst_port);

  /* calculate the checksum */
  *(uint16_t*)(pkt + 70) = 0; /* clear from previous runs */
  sum = ip_csum(0, pkt + 22, 32); /* pseudo-header: addrs */
  tmp = htonl(40);
  sum = ip_csum(sum, &tmp, sizeof(tmp)); /* TCPv6 len */
  tmp = htonl(6);
  sum = ip_csum(sum, &tmp, sizeof(tmp)); /* TCP next type */
  sum = ip_csum(sum, pkt + 54, 40);
  *(uint16_t*)(pkt + 70) = ip_csum_fold(sum);

  *len = sizeof(pkt);
  return pkt;
}

static struct frame_builder frame_builders[] = {
  {
    .category = 0,
    .options = 0,
    .build = gen_custom_frame,
  },
  {
    .category = CAT_ARPREQ,
    .options = IP_DST_SWEEP | REQUIRES_IP4,
    .build = gen_arp_req,
  },
  {
    .category = CAT_LLDISCO,
    .options = REQUIRES_IP4,
    .build = gen_mdns4_req,
  },
  {
    .category = CAT_LLDISCO,
    .options = REQUIRES_IP4,
    .build = gen_ll_icmp4_req,
  },
  {
    .category = CAT_LLDISCO,
    .options = REQUIRES_IP6,
    .build = gen_ll_icmp6_req,
  },
  {
    .category = CAT_LLDISCO,
    .options = REQUIRES_IP6,
    .build = gen_mdns6_req,
  },
  {
    .category = CAT_ICMPECHO,
    .options = IP_DST_SWEEP | REQUIRES_IP4,
    .build = gen_icmp4_req,
  },
  {
    .category = CAT_ICMPECHO,
    .options = IP_DST_SWEEP | REQUIRES_IP6,
    .build = gen_icmp6_req,
  },
  {
    .category = CAT_TCPSYN,
    .options = IP_PORT_SWEEP | REQUIRES_IP4,
    .build = gen_tcp4_syn,
  },
  {
    .category = CAT_TCPSYN,
    .options = IP_PORT_SWEEP | REQUIRES_IP6,
    .build = gen_tcp6_syn,
  }
};

static int is_src_family_allowed(struct frame_builder *fb,
    struct frameconf *cfg) {
  if (fb->options & REQUIRES_IP4 &&
      cfg->src_ip.u.sa.sa_family != AF_INET) {
    return 0;
  } else if (fb->options & REQUIRES_IP6 &&
      cfg->src_ip.u.sa.sa_family != AF_INET6) {
    return 0;
  }
  return 1;
}

static int is_family_allowed(struct frame_builder *fb, struct frameconf *cfg) {

  if (cfg->src_ip.u.sa.sa_family != cfg->curr_dst_ip.u.sa.sa_family) {
    return 0;
  } else if (fb->options & REQUIRES_IP4 &&
      cfg->curr_dst_ip.u.sa.sa_family != AF_INET) {
    return 0;
  } else if (fb->options & REQUIRES_IP6 &&
      cfg->curr_dst_ip.u.sa.sa_family != AF_INET6) {
    return 0;
  } else if (cfg->curr_dst_ip.u.sa.sa_family != AF_INET &&
      cfg->curr_dst_ip.u.sa.sa_family != AF_INET6) {
    return 0;
  }
  return 1;
}

static void next_builder(struct frameconf *cfg) {
  port_ranges_reset(&cfg->dst_ports);
  ip_blocks_reset(&cfg->dst_ips);
  cfg->curr_dst_ip.u.sa.sa_family = AF_UNSPEC;
  cfg->curr_buildix++;
  cfg->curr_buildstate = 0;
}

static const char *get_next_frame(struct frameconf *cfg, size_t *len) {
  struct frame_builder *fb;
  const char *ret;

again:
  if (cfg->curr_buildix >=
      sizeof(frame_builders) / sizeof(struct frame_builder)) {
    return NULL;
  }

  fb = frame_builders + cfg->curr_buildix;

  if (fb->category && !(fb->category & cfg->categories)) {
    next_builder(cfg);
    goto again;
  }

  /* advance addresses on sweep (TODO: simplify) */
next_addr:
  if (fb->options & IP_PORT_SWEEP) {
    while (!port_ranges_next(&cfg->dst_ports, &cfg->curr_dst_port)) {
      if (!ip_blocks_next(&cfg->dst_ips, &cfg->curr_dst_ip)) {
        next_builder(cfg);
        goto again;
      }
    }

    if (cfg->curr_dst_ip.u.sa.sa_family == AF_UNSPEC) {
      /* initialize dst addr on first run of this builder */
      if (!ip_blocks_next(&cfg->dst_ips, &cfg->curr_dst_ip)) {
        next_builder(cfg);
        goto again;
      }
    }
    if (!is_family_allowed(fb, cfg)) {
      goto next_addr;
    }
  } else if (fb->options & IP_DST_SWEEP) {
    if (!ip_blocks_next(&cfg->dst_ips, &cfg->curr_dst_ip)) {
      next_builder(cfg);
      goto again;
    }
    if (!is_family_allowed(fb, cfg)) {
      goto next_addr;
    }
  } else {
    /* ll-disco, &c */
    if (!is_src_family_allowed(fb, cfg)) {
      next_builder(cfg);
      goto again;
    }
  }

  ret = fb->build(cfg, len);
  if (ret == NULL) {
    next_builder(cfg);
    goto again;
  }

  cfg->curr_buildstate++;
  return ret;
}

static struct eth_sender *get_sender_by_ifname(const char *ifname) {
  int i;
  struct eth_sender *ret = NULL;

  for (i = 0; i < ethframe_ctx.nifs; i++) {
    if (strcmp(ethframe_ctx.ifs[i], ifname) == 0) {
      ret = &ethframe_ctx.senders[i];
      break;
    }
  }

  return ret;
}

int ethframe_init(struct eds_service *svc) {
  struct iface ifs;
  struct iface_entry ent;
  int i;
  int ret;

  ret = iface_init(&ifs);
  if (ret < 0) {
    ylog_error("%s: iface_init: %s", svc->name, iface_strerror(&ifs));
    return -1;
  }

  for (i = 0; i < NBR_IFACES; i++) {
next_iface:
    ret = iface_next(&ifs, &ent);
    if (ret <= 0) {
      break;
    }

    /* skip loopbacks, interfaces like pflogN, &c */
    if (memcmp(ent.addr, "\0\0\0\0\0\0", IFACE_ADDRSZ) == 0) {
      goto next_iface;
    }

    if (eth_sender_init(&ethframe_ctx.senders[i], ent.name) < 0) {
      /* the interface may not be configured, so this should not be fatal */
      goto next_iface;
    }

    snprintf(ethframe_ctx.ifs[i], sizeof(ethframe_ctx.ifs[i]), "%s", ent.name);
    ethframe_ctx.nifs++;
    ylog_info("%s: initialized iface \"%s\"", svc->name, ent.name);
  }

  iface_cleanup(&ifs);
  return 0;
}

void ethframe_fini(struct eds_service *svc) {
  int i;

  for (i = 0; i < ethframe_ctx.nifs; i++) {
    eth_sender_cleanup(&ethframe_ctx.senders[i]);
  }
}

static void write_ok_response(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  struct ycl_msg_status_resp resp = {0};
  int ret;

  eds_client_set_ticker(cli, NULL);
  eds_client_clear_actions(cli);
  resp.okmsg = "ok";
  ret = ycl_msg_create_status_resp(&ecli->msgbuf, &resp);
  if (ret != YCL_OK) {
    ylog_error("ethframecli%d: error response serialization error", fd);
    return;
  }

  eds_client_send(cli, ycl_msg_bytes(&ecli->msgbuf),
      ycl_msg_nbytes(&ecli->msgbuf), NULL);
}

static void write_err_response(struct eds_client *cli, int fd,
    const char *errmsg) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  struct ycl_msg_status_resp resp = {0};
  int ret;

  eds_client_set_ticker(cli, NULL);
  ylog_error("ethframecli%d: %s", fd, errmsg);
  eds_client_clear_actions(cli);
  resp.errmsg = errmsg;
  ret = ycl_msg_create_status_resp(&ecli->msgbuf, &resp);
  if (ret != YCL_OK) {
    ylog_error("ethframecli%d: OK response serialization error", fd);
  } else {
    eds_client_send(cli, ycl_msg_bytes(&ecli->msgbuf),
        ycl_msg_nbytes(&ecli->msgbuf), NULL);
  }
}

static void on_term(struct eds_client *cli, int fd) {
  eds_client_clear_actions(cli);
}

static int setup_frameconf(struct frameconf *cfg,
    struct ycl_msg_ethframe_req *req, char *errbuf, size_t errbuflen) {
  size_t failoff;
  struct flagset_result fsres = {0};

  /* clear from previous runs */
  memset(cfg, 0, sizeof(*cfg));

  cfg->custom_frames = req->custom_frames;
  cfg->ncustom_frames = req->ncustom_frames;
  cfg->curr_custom_frame = 0;

  if (req->categories != NULL) {
    if (flagset_from_str(category_flags, req->categories, &fsres) < 0) {
      snprintf(errbuf, errbuflen, "invalid categories, offset %zu: %s",
          fsres.erroff, fsres.errmsg);
      return -1;
    }
    cfg->categories = fsres.flags;
  }

  if (req->pps > 0 && req->pps <= UINT_MAX) {
    cfg->pps = (unsigned int)req->pps;
  }

  if (req->iface == NULL) {
    snprintf(errbuf, errbuflen, "missing iface");
    return -1;
  }
  cfg->iface = req->iface;

  if (req->eth_src != NULL) {
    if (eth_parse_addr(cfg->eth_src, sizeof(cfg->eth_src), req->eth_src) < 0) {
      snprintf(errbuf, errbuflen, "invalid eth src");
      return -1;
    }
  } else if (cfg->ncustom_frames == 0 && cfg->categories != 0) {
    snprintf(errbuf, errbuflen, "missing eth src");
    return -1;
  }

  if (req->eth_dst != NULL) {
    if (eth_parse_addr(cfg->eth_dst, sizeof(cfg->eth_dst), req->eth_dst) < 0) {
      snprintf(errbuf, errbuflen, "invalid eth dst");
      return -1;
    }
  }
  /* we don't require eth_dstlen to be set- the frames may have them (e.g.,
   * bcasts) */

  if (req->ip_src != NULL) {
    if (ip_addr(&cfg->src_ip, req->ip_src, NULL) < 0) {
      snprintf(errbuf, errbuflen, "invalid ip src address");
      return -1;
    }
  } else if (cfg->ncustom_frames == 0 && cfg->categories != 0) {
    snprintf(errbuf, errbuflen, "missing ip src address");
    return -1;
  }

  if (req->ip_dsts != NULL) {
    if (ip_blocks_init(&cfg->dst_ips, req->ip_dsts, NULL) < 0) {
      snprintf(errbuf, errbuflen, "invalid ip dst addresses");
      return -1;
    }
  } else if (cfg->ncustom_frames == 0 && !(cfg->categories | CAT_LLDISCO)) {
    snprintf(errbuf, errbuflen, "missing ip dst addresses");
    return -1;
  }

  if (req->port_dsts != NULL) {
    if (port_ranges_from_str(&cfg->dst_ports, req->port_dsts, &failoff) < 0) {
      snprintf(errbuf, errbuflen, "dst ports: syntax error near %zu", failoff);
      return -1;
    }
  }

  return 0;
}

static void cleanup_frameconf(struct frameconf *cfg) {
  ip_blocks_cleanup(&cfg->dst_ips);
  port_ranges_cleanup(&cfg->dst_ports);
}

/* returns -1 on failure, 0 when done, 1 if more things can be sent */
static int write_npackets(struct eds_client *cli, int npackets, char *errbuf,
    size_t errbuflen) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  const char *frame;
  size_t framelen;
  int ret;

  while (npackets > 0) {
    frame = get_next_frame(&ecli->cfg, &framelen);
    if (frame == NULL) {
      return 0;
    }

    ret = eth_sender_write(ecli->sender, frame, framelen);
    if (ret < 0) {
      snprintf(errbuf, errbuflen, "error sending frame: %s",
          eth_sender_strerror(ecli->sender));
      return -1;
    }

    npackets--;
  }

  return 1;
}

static void on_write_frames(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  char errbuf[128];
  int ret;

  if (ecli->tppcount > 0) {
    ecli->tppcount--;
    return;
  } else {
    ecli->tppcount = ecli->tpp;
  }

  ret = write_npackets(cli, ecli->npackets, errbuf, sizeof(errbuf));
  if (ret < 0) {
    write_err_response(cli, fd, errbuf);
  } else if (ret == 0) {
    write_ok_response(cli, fd);
  }
}

static void on_read_req(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  int ret;
  const char *errmsg = "an internal error occurred";
  char errbuf[128];
  unsigned int tps;
  struct ycl_msg_ethframe_req req = {0};

  ret = ycl_recvmsg(&ecli->ycl, &ecli->msgbuf);
  if (ret == YCL_AGAIN) {
    return;
  } else if (ret != YCL_OK) {
    errmsg = ycl_strerror(&ecli->ycl);
    goto fail;
  }

  ret = ycl_msg_parse_ethframe_req(&ecli->msgbuf, &req);
  if (ret != YCL_OK) {
    errmsg = "request parse error";
    goto fail;
  }

  ret = setup_frameconf(&ecli->cfg, &req, errbuf, sizeof(errbuf));
  if (ret < 0) {
    errmsg = errbuf;
    goto fail;
  }

  ecli->sender = get_sender_by_ifname(ecli->cfg.iface);
  if (ecli->sender == NULL) {
    snprintf(errbuf, sizeof(errbuf), "iface \"%s\" does not exist", req.iface);
    errmsg = errbuf;
    goto fail;
  }

  ylog_info("ethframecli%d: iface:\"%s\"", fd, ecli->cfg.iface);

  eds_client_set_on_readable(cli, on_term, EDS_DEFER);

  if (ecli->cfg.pps > 0) {
    tps = 1000000 / cli->svc->tick_slice_us; /* ticks per second */
    assert(tps > 0);
    if (ecli->cfg.pps < tps) {
      /* we require multiple ticks for sending one packet */
      ecli->tppcount = ecli->tpp = tps / ecli->cfg.pps;
      ecli->npackets = 1;
    } else {
      /* we require one tick for sending N packets ( N >= 1) */
      ecli->tppcount = ecli->tpp = 0;
      ecli->npackets = ecli->cfg.pps / tps;
    }
    eds_client_set_ticker(cli, on_write_frames);
  } else {
    ecli->tppcount = ecli->tpp = 0;
    ecli->npackets = YIELD_NPACKETS;
    eds_client_set_on_writable(cli, on_write_frames, 0);
  }
  return;

fail:
  write_err_response(cli, fd, errmsg);
}

void ethframe_on_readable(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  int ret;

  ycl_init(&ecli->ycl, fd);
  if (!(ecli->flags & CLIFLAG_HASMSGBUF)) {
    ret = ycl_msg_init(&ecli->msgbuf);
    if (ret != YCL_OK) {
      ylog_error("ethframecli%d: ycl_msg_init failure", fd);
      goto fail;
    }
    ecli->flags |= CLIFLAG_HASMSGBUF;
  }

  ecli->tpp = 0;
  ecli->tppcount = 0;
  ecli->npackets = 0;
  eds_client_set_on_readable(cli, on_read_req, 0);
  return;
fail:
  eds_service_remove_client(cli->svc, cli);
}

void ethframe_on_done(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  ylog_info("ethframecli%d: done", fd);
  cleanup_frameconf(&ecli->cfg);
  ycl_msg_reset(&ecli->msgbuf);
}

void ethframe_on_finalize(struct eds_client *cli) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  if (ecli->flags & CLIFLAG_HASMSGBUF) {
    ycl_msg_cleanup(&ecli->msgbuf);
  }
}
