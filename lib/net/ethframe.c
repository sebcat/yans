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
