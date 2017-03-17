#ifndef ETHERFRAME_H_
#define ETHERFRAME_H_

#include <stdint.h>
#include <stddef.h>

/* maximum size of ethernet frames built by ethframe code */
#define ETHFRAME_FRAMESZ_MAX 256

struct ethframe_icmp4_opts {
  uint8_t eth_dst[6];
  uint8_t eth_src[6];
  uint32_t ip_src;
  uint32_t ip_dst;
};

struct ethframe {
  size_t len;
  uint8_t buf[ETHFRAME_FRAMESZ_MAX];
};

void ethframe_icmp4_init(struct ethframe *f,
    const struct ethframe_icmp4_opts *opts);

#endif /* ETHERFRAME_H_ */
