/* etherframe.c - beware of dragons */
#include <string.h>

#include <arpa/inet.h>

#include <lib/net/ethframe.h>

#define ETHFRAME_ETHERSZ 14
#define ETHFRAME_ETHER                                    \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* eth dst */     \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* eth src */     \
    0x00, 0x00                          /* type */

#define ETHFRAME_ETHDSTOFF   0
#define ETHFRAME_ETHSRCOFF   6
#define ETHFRAME_ETHTYPEOFF 12

#define ETHFRAME_SETETHTYPE(f, type) \
    ((f)->buf[ETHFRAME_ETHTYPEOFF]) = (((type)>>8)&0xff);  \
    ((f)->buf[ETHFRAME_ETHTYPEOFF+1]) = ((type)&0xff)

#define ETHFRAME_IPV4SZ 20
#define ETHFRAME_IPV4                                                       \
    0x45,       /* version 4, hdrlen: 5*4 = 20 */                           \
    0x00,       /* DSCP (diffserv), ECN (expl. congestion notification) */  \
    0x00, 0x00, /* total length (ipv4 header + data) */                     \
    0x00, 0x00, /* ID */                                                    \
    0x00, 0x00, /* flags, fragment offset */                                \
    0x00,       /* TTL */                                                   \
    0x00,       /* protocol */                                              \
    0x00, 0x00, /* header checksum*/                                        \
    0x00, 0x00, 0x00, 0x00, /* source */                                    \
    0x00, 0x00, 0x00, 0x00  /* destination */

#define ETHFRAME_IPV4LENOFF   2
#define ETHFRAME_IPV4TTLOFF   8
#define ETHFRAME_IPV4PROTOOFF 9
#define ETHFRAME_IPV4SRCOFF   12
#define ETHFRAME_IPV4DSTOFF   16

#define ETHFRAME_SETIPV4LEN(f, base, len) \
    ((f)->buf[(base) + ETHFRAME_IPV4LENOFF]) = (((len)>>8)&0xff); \
    ((f)->buf[(base) + ETHFRAME_IPV4LENOFF + 1]) = ((len)&0xff);
#define ETHFRAME_SETIPV4TTL(f, base, proto) \
    (*((f)->buf + (base) + ETHFRAME_IPV4TTLOFF) = (proto))
#define ETHFRAME_SETIPV4PROTO(f, base, proto) \
    (*((f)->buf + (base) + ETHFRAME_IPV4PROTOOFF) = (proto))
#define ETHFRAME_SETIPV4SRC(f, base, addr) \
    memcpy(f->buf + (base) + ETHFRAME_IPV4SRCOFF, addr, sizeof(uint32_t));
#define ETHFRAME_SETIPV4DST(f, base, addr) \
    memcpy(f->buf + (base) + ETHFRAME_IPV4DSTOFF, addr, sizeof(uint32_t));
#define ETHFRAME_SETIPV4CSUM(f, base)                                     \
    do {                                                                  \
      uint32_t sum = 0;                                                   \
      uint16_t *curr = (uint16_t*)&(f)->buf[(base)];                      \
      sum += *curr; sum += *(curr+1); sum += *(curr+2); sum += *(curr+3); \
      sum += *(curr+4);/*skip csum*/; sum += *(curr+6); sum += *(curr+7); \
      sum += *(curr+8); sum += *(curr+9);                                 \
      *(uint16_t*)(curr+5) = ~((sum&0xffff)+(sum >> 16));                 \
    } while(0);

#define ETHFRAME_IPV6SZ 40
#define ETHFRAME_IPV6                                       \
    0x60, 0x00, 0x00, 0x00, /* ver, TC, flow label */       \
    0x00, 0x00,             /* payload len */               \
    0x00,                   /* next header */               \
    0x00,                   /* hop limit */                 \
    /* src addr */                                          \
    0x00, 0x00, 0x00, 0x00,                                 \
    0x00, 0x00, 0x00, 0x00,                                 \
    0x00, 0x00, 0x00, 0x00,                                 \
    0x00, 0x00, 0x00, 0x00,                                 \
    /* dst addr */                                          \
    0x00, 0x00, 0x00, 0x00,                                 \
    0x00, 0x00, 0x00, 0x00,                                 \
    0x00, 0x00, 0x00, 0x00,                                 \
    0x00, 0x00, 0x00, 0x00

#define ETHFRAME_IPV6LENOFF   4
#define ETHFRAME_IPV6NXTOFF   6
#define ETHFRAME_IPV6HLIMOFF  7
#define ETHFRAME_IPV6SRCOFF   8
#define ETHFRAME_IPV6DSTOFF  24

/* initial pseudo header checksum, *not* shift-added and one-complemented.
 * The length information assumes that no exta IPv6 headers are present.
 * res must be of type uint32_t. */
#define ETHFRAME_IPV6PSUM(f_, base_, sum_)                                   \
    do {                                                                     \
      int i_ = 0;                                                            \
      (sum_) = 0;                                                            \
      uint16_t *curr_ = (uint16_t*)&(f_)->buf[(base_) + ETHFRAME_IPV6SRCOFF];\
      for (i_ = 0; i_ < 16; i_++) {                                          \
        (sum_) += *curr_;                                                    \
        curr_++;                                                             \
      }                                                                      \
      curr_ = (uint16_t*)&(f_)->buf[(base_) + ETHFRAME_IPV6LENOFF];          \
      sum_ += *curr_;                                                        \
      sum_ += htons((uint16_t)(f_)->buf[(base_) + ETHFRAME_IPV6NXTOFF]);     \
    } while(0);


#define ETHFRAME_SETIPV6LEN(f, base, len) \
    (f)->buf[(base) + ETHFRAME_IPV6LENOFF] = (((len)>>8)&0xff); \
    (f)->buf[(base) + ETHFRAME_IPV6LENOFF + 1] = ((len)&0xff)
#define ETHFRAME_SETIPV6NXT(f, base, nxt) \
    (*((f)->buf + (base) + ETHFRAME_IPV6NXTOFF) = (nxt))
#define ETHFRAME_SETIPV6HLIM(f, base, lim) \
    (*((f)->buf + (base) + ETHFRAME_IPV6HLIMOFF) = (lim))

#define ETHFRAME_ICMP4_EREQ                  \
    0x08,        /* type: echo request */    \
    0x00,        /* code: 0 */               \
    0x00, 0x00,  /* checksum */              \
    0x00, 0x00,  /* id */                    \
    0x00, 0x00,  /* seq */                   \
    /* data */                               \
    0x00, 0x01, 0x02, 0x03,                  \
    0xff, 0xfe, 0xfd, 0xfc

#define ETHFRAME_ICMP6_EREQ                  \
    0x80,        /* type: echo request */    \
    0x00,        /* code */                  \
    0x00, 0x00,  /* checksum*/               \
    0x00, 0x00,  /* id */                    \
    0x00, 0x00,  /* seq */                   \
    /* data */                               \
    0x00, 0x01, 0x02, 0x03,                  \
    0xff, 0xfe, 0xfd, 0xfc

#define ETHFRAME_SETICMP4CSUM(f, base)                                    \
    do {                                                                  \
      uint32_t sum = 0;                                                   \
      uint16_t *curr =                                                    \
          (uint16_t*)(f->buf+(base));                                     \
      sum += *curr; /* skip csum */ sum += *(curr+2); sum += *(curr+3);   \
      sum += *(curr+4); sum += *(curr+5); sum += *(curr+6);               \
      sum += *(curr+7);                                                   \
      *(curr+1) = ~((sum&0xffff)+(sum >> 16));                            \
    } while(0);

#define ETHFRAME_SETICMP6CSUM(f_, base_, sum_)                            \
    do {                                                                  \
      uint16_t *curr_ = (uint16_t*)((f_)->buf+(base_));                   \
      sum_ += *curr_;     sum_ += *(curr_+2); sum_ += *(curr_+3);         \
      sum_ += *(curr_+4); sum_ += *(curr_+5); sum_ += *(curr_+6);         \
      sum_ += *(curr_+7);                                                 \
      *(curr_+1) = ~((sum_&0xffff)+(sum_>>16));                           \
    } while(0);

#define ETHFRAME_SSDP4 \
    0x90, 0x83,  /* source port */                                          \
    0x07, 0x6c,  /* dst port: 1900 */                                       \
    0x00, 0x94,  /* length: 148 */                                          \
    0xaa, 0x78,  /* checksum */                                             \
    /* payload:                                                             \
         M-SEARCH * HTTP/1.1                                                \
         HOST: 239.255.255.250:1900                                         \
         MAN: ssdp:discover                                                 \
         MX: 10                                                             \
         ST: ssdp:all                                                       \
         User-Agent: DinMammaOchDanHarmon/1.0 UPnP/1.1 */                   \
    0x4d, 0x2d, 0x53, 0x45, 0x41, 0x52, 0x43, 0x48, 0x20, 0x2a, 0x20, 0x48, \
    0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31, 0x0d, 0x0a, 0x48, 0x4f, 0x53, \
    0x54, 0x3a, 0x20, 0x32, 0x33, 0x39, 0x2e, 0x32, 0x35, 0x35, 0x2e, 0x32, \
    0x35, 0x35, 0x2e, 0x32, 0x35, 0x30, 0x3a, 0x31, 0x39, 0x30, 0x30, 0x0d, \
    0x0a, 0x4d, 0x41, 0x4e, 0x3a, 0x20, 0x73, 0x73, 0x64, 0x70, 0x3a, 0x64, \
    0x69, 0x73, 0x63, 0x6f, 0x76, 0x65, 0x72, 0x0d, 0x0a, 0x4d, 0x58, 0x3a, \
    0x20, 0x31, 0x30, 0x0d, 0x0a, 0x53, 0x54, 0x3a, 0x20, 0x73, 0x73, 0x64, \
    0x70, 0x3a, 0x61, 0x6c, 0x6c, 0x0d, 0x0a, 0x55, 0x73, 0x65, 0x72, 0x2d, \
    0x41, 0x67, 0x65, 0x6e, 0x74, 0x3a, 0x20, 0x44, 0x69, 0x6e, 0x4d, 0x61, \
    0x6d, 0x6d, 0x61, 0x4f, 0x63, 0x68, 0x44, 0x61, 0x6e, 0x48, 0x61, 0x72, \
    0x6d, 0x6f, 0x6e, 0x2f, 0x31, 0x2e, 0x30, 0x20, 0x55, 0x50, 0x6e, 0x50, \
    0x2f, 0x31, 0x2e, 0x31, 0x0d, 0x0a, 0x0d, 0x0a

#define ETHFRAME_MDNS4 \
    0x14, 0xe9,  /* src port: 5353 */                 \
    0x14, 0xe9,  /* dst port: 5353 */                 \
    0x00, 0x30,  /* length: 48 */                     \
    0x00, 0x00,  /* csum */                           \
    /* MDNS query */                                  \
    0x00, 0x00,  /* transaction ID */                 \
    0x00, 0x00,  /* flags (TODO: maybe set some?) */  \
    0x00, 0x01,  /* # questions: 1 */                 \
    0x00, 0x00,  /* answer RRs: 0 */                  \
    0x00, 0x00,  /* auth RRs: 0 */                    \
    0x00, 0x00,  /* additional RRs: 0 */              \
    /* TODO: support multiple names, e.g., 
     *         - _services._dns-sd._udp.local
     *         - */
static const uint8_t ethframe_udp4_mdns_frame[] = {

};

static const uint8_t ethframe_udp4_ssdp_frame[] = {
  ETHFRAME_ETHER,
  ETHFRAME_IPV4,
  ETHFRAME_SSDP4
};

/* TODO: opts, checksum */
void ethframe_udp4_ssdp_init(struct ethframe *f) {
  uint32_t ip_src = 0x1200a8c0;
  uint32_t ip_dst = 0xfaffffef; /* 239.255.255.250 */

  f->len = sizeof(ethframe_udp4_ssdp_frame);
  memcpy(f->buf, ethframe_udp4_ssdp_frame, sizeof(ethframe_udp4_ssdp_frame));
  /* ipv4 mcast 7f:ff:fa */
  memcpy(f->buf + ETHFRAME_ETHDSTOFF, "\x01\x00\x5e\x7f\xff\xfa", 6);
  memcpy(f->buf + ETHFRAME_ETHSRCOFF, "\x00\x24\xd7\x17\x9c\x38", 6);
  ETHFRAME_SETETHTYPE(f, 0x0800); /* IPv4 */

  ETHFRAME_SETIPV4LEN(f, ETHFRAME_ETHERSZ,
      sizeof(ethframe_udp4_ssdp_frame) - ETHFRAME_ETHERSZ);
  ETHFRAME_SETIPV4TTL(f, ETHFRAME_ETHERSZ, 1);
  ETHFRAME_SETIPV4PROTO(f, ETHFRAME_ETHERSZ, 17); /* UDP */
  ETHFRAME_SETIPV4SRC(f, ETHFRAME_ETHERSZ, &ip_src);
  ETHFRAME_SETIPV4DST(f, ETHFRAME_ETHERSZ, &ip_dst);
  ETHFRAME_SETIPV4CSUM(f, ETHFRAME_ETHERSZ);
}

static const uint8_t ethframe_icmp4_ereq_frame[] = {
  ETHFRAME_ETHER,
  ETHFRAME_IPV4,
  ETHFRAME_ICMP4_EREQ
};

void ethframe_icmp4_ereq_init(struct ethframe *f,
    const struct ethframe_icmp4_ereq_opts *opts) {

  f->len = sizeof(ethframe_icmp4_ereq_frame);
  memcpy(f->buf, ethframe_icmp4_ereq_frame, sizeof(ethframe_icmp4_ereq_frame));
  memcpy(f->buf + ETHFRAME_ETHDSTOFF, opts->eth_dst, sizeof(opts->eth_dst));
  memcpy(f->buf + ETHFRAME_ETHSRCOFF, opts->eth_src, sizeof(opts->eth_src));
  ETHFRAME_SETETHTYPE(f, 0x0800); /* IPv4 */

  ETHFRAME_SETIPV4LEN(f, ETHFRAME_ETHERSZ,
      sizeof(ethframe_icmp4_ereq_frame) - ETHFRAME_ETHERSZ);
  ETHFRAME_SETIPV4TTL(f, ETHFRAME_ETHERSZ, opts->ip_ttl);
  ETHFRAME_SETIPV4PROTO(f, ETHFRAME_ETHERSZ, 1); /* ICMP */
  ETHFRAME_SETIPV4SRC(f, ETHFRAME_ETHERSZ, &opts->ip_src);
  ETHFRAME_SETIPV4DST(f, ETHFRAME_ETHERSZ, &opts->ip_dst);
  ETHFRAME_SETIPV4CSUM(f, ETHFRAME_ETHERSZ);

  ETHFRAME_SETICMP4CSUM(f, ETHFRAME_ETHERSZ + ETHFRAME_IPV4SZ);
}

static const uint8_t ethframe_icmp6_ereq_frame[] = {
  ETHFRAME_ETHER,
  ETHFRAME_IPV6,
  ETHFRAME_ICMP6_EREQ
};

void ethframe_icmp6_ereq_init(struct ethframe *f,
    const struct ethframe_icmp6_ereq_opts *opts) {

  uint32_t sum;

  f->len = sizeof(ethframe_icmp6_ereq_frame);
  memcpy(f->buf, ethframe_icmp6_ereq_frame, sizeof(ethframe_icmp6_ereq_frame));
  memcpy(f->buf + ETHFRAME_ETHDSTOFF, opts->eth_dst, sizeof(opts->eth_dst));
  memcpy(f->buf + ETHFRAME_ETHSRCOFF, opts->eth_src, sizeof(opts->eth_src));
  ETHFRAME_SETETHTYPE(f, 0x86dd); /* IPv6 */

  ETHFRAME_SETIPV6LEN(f, ETHFRAME_ETHERSZ,
      sizeof(ethframe_icmp6_ereq_frame) -
      ETHFRAME_ETHERSZ -
      ETHFRAME_IPV6SZ);
  ETHFRAME_SETIPV6NXT(f, ETHFRAME_ETHERSZ, 58); /* ICMPv6 */
  ETHFRAME_SETIPV6HLIM(f, ETHFRAME_ETHERSZ, opts->ip_hoplim);
  memcpy(f->buf + ETHFRAME_ETHERSZ + ETHFRAME_IPV6SRCOFF,
      opts->ip_src, sizeof(opts->ip_src));
  memcpy(f->buf + ETHFRAME_ETHERSZ + ETHFRAME_IPV6DSTOFF,
      opts->ip_dst, sizeof(opts->ip_dst));

  ETHFRAME_IPV6PSUM(f, ETHFRAME_ETHERSZ, sum);
  ETHFRAME_SETICMP6CSUM(f, ETHFRAME_ETHERSZ + ETHFRAME_IPV6SZ, sum);
}
