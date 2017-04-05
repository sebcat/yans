#ifndef ETHERFRAME_H_
#define ETHERFRAME_H_

#include <stdint.h>
#include <stddef.h>

/* maximum size of ethernet frames built by ethframe code */
#define ETHFRAME_FRAMESZ_MAX 512

struct ethframe_icmp4_ereq_opts {
  uint8_t eth_dst[6];
  uint8_t eth_src[6];
  uint32_t ip_src;
  uint32_t ip_dst;
  uint8_t ip_ttl;
};

struct ethframe_icmp6_ereq_opts {
  uint8_t eth_dst[6];
  uint8_t eth_src[6];
  uint8_t ip_src[16];
  uint8_t ip_dst[16];
  uint8_t ip_hoplim;
};

struct ethframe {
  size_t len;
  uint8_t buf[ETHFRAME_FRAMESZ_MAX];
};

void ethframe_icmp4_ereq_init(struct ethframe *f,
    const struct ethframe_icmp4_ereq_opts *opts);

void ethframe_icmp6_ereq_init(struct ethframe *f,
    const struct ethframe_icmp6_ereq_opts *opts);

void ethframe_udp4_ssdp_init(struct ethframe *f);

#endif /* ETHERFRAME_H_ */
