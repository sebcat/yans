/* etherframe.c - beware of dragons */
#include <string.h>

#include <arpa/inet.h>

#include <lib/net/etherframe.h>

#define ETHFRAME_ETHERSZ 14
#define ETHFRAME_ETHER \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* eth dst */     \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* eth src */     \
    0x00, 0x00                          /* type */

#define ETHFRAME_ETHDSTOFF   0
#define ETHFRAME_ETHSRCOFF   6
#define ETHFRAME_ETHTYPEOFF 12


#define ETHFRAME_IPV4SZ 20
#define ETHFRAME_IPV4 \
  0x45,       /* version 4, hdrlen: 5*4 = 20 */ \
  0x00,       /* DSCP (diffserv), ECN (expl. congestion notification) */  \
  0x00, 0x00, /* total length (ipv4 header + data) */                     \
  0x00, 0x00, /* ID */                                                    \
  0x00, 0x00, /* flags, fragment offset */                                \
  0x80,       /* TTL */                                                   \
  0x00,       /* protocol */                                              \
  0x00, 0x00, /* header checksum*/                                        \
  0x00, 0x00, 0x00, 0x00, /* source */                                    \
  0x00, 0x00, 0x00, 0x00  /* destination */

#define ETHFRAME_SETIPV4CSUM(f)                                           \
    do {                                                                  \
      uint32_t sum = 0;                                                   \
      uint16_t *curr = (uint16_t*)&f->buf[ETHFRAME_ETHERSZ];              \
      sum += *curr; sum += *(curr+1); sum += *(curr+2); sum += *(curr+3); \
      sum += *(curr+4);/*skip csum*/; sum += *(curr+6); sum += *(curr+7); \
      sum += *(curr+8); sum += *(curr+9);                                 \
      *(uint16_t*)(curr+5) = ~((sum&0xffff)+(sum >> 16));                 \
    } while(0);

#define ETHFRAME_IPV4LENOFF   (ETHFRAME_ETHERSZ + 2)
#define ETHFRAME_IPV4PROTOOFF (ETHFRAME_ETHERSZ + 9)
#define ETHFRAME_IPV4SRCOFF   (ETHFRAME_ETHERSZ + 12)
#define ETHFRAME_IPV4DSTOFF   (ETHFRAME_ETHERSZ + 16)

#define ETHFRAME_SETETHTYPE(f, type) \
    (*(uint16_t*)((f)->buf + ETHFRAME_ETHTYPEOFF) = (type))

#define ETHFRAME_SETIPV4LEN(f, len) \
    (*(uint16_t*)((f)->buf + ETHFRAME_IPV4LENOFF) = (len))
#define ETHFRAME_SETIPV4PROTO(f, proto) \
    (*((f)->buf + ETHFRAME_IPV4PROTOOFF) = (proto))
#define ETHFRAME_SETIPV4SRC(f, addr) \
    (*(uint32_t*)((f)->buf + ETHFRAME_IPV4SRCOFF) = (addr))
#define ETHFRAME_SETIPV4DST(f, addr) \
    (*(uint32_t*)((f)->buf + ETHFRAME_IPV4DSTOFF) = (addr))

static uint8_t ethframe_icmp4_frame[] = {
  ETHFRAME_ETHER,
  ETHFRAME_IPV4,
  0x08,        /* type: echo request */
  0x00,        /* code: 0 */
  0x00, 0x00,  /* checksum */
  0x00, 0x00,  /* id */
  0x00, 0x00,  /* seq */
  /* data */
  0x00, 0x01, 0x02, 0x03,
  0xff, 0xfe, 0xfd, 0xfc,
};

#define ETHFRAME_SETICMP4CSUM(f)                                          \
    do {                                                                  \
      uint32_t sum = 0;                                                   \
      uint16_t *curr =                                                    \
          (uint16_t*)&f->buf[ETHFRAME_ETHERSZ+ETHFRAME_IPV4SZ];           \
      sum += *curr; /* skip csum */ sum += *(curr+2); sum += *(curr+3);   \
      sum += *(curr+4); sum += *(curr+5); sum += *(curr+6);               \
      sum += *(curr+7);                                                   \
      *(uint16_t*)(curr+1) = ~((sum&0xffff)+(sum >> 16));                 \
    } while(0);

void ethframe_icmp4_init(struct ethframe *f,
    const struct ethframe_icmp4_opts *opts) {
  f->len = sizeof(ethframe_icmp4_frame);
  memcpy(f->buf, ethframe_icmp4_frame, sizeof(ethframe_icmp4_frame));
  memcpy(f->buf + ETHFRAME_ETHDSTOFF, opts->eth_dst, sizeof(opts->eth_dst));
  memcpy(f->buf + ETHFRAME_ETHSRCOFF, opts->eth_src, sizeof(opts->eth_src));

  ETHFRAME_SETETHTYPE(f, htons(0x0800)); /* IPv4 */
  ETHFRAME_SETIPV4LEN(f,
      htons(sizeof(ethframe_icmp4_frame) - ETHFRAME_ETHERSZ));
  ETHFRAME_SETIPV4PROTO(f, 1); /* ICMP */
  ETHFRAME_SETIPV4SRC(f, opts->ip_src);
  ETHFRAME_SETIPV4DST(f, opts->ip_dst);
  ETHFRAME_SETIPV4CSUM(f);
  ETHFRAME_SETICMP4CSUM(f);
}
