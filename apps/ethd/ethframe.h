#ifndef ETHD_ETHFRAME_H__
#define ETHD_ETHFRAME_H__

#include <stdint.h>

#include <lib/net/eth.h>
#include <lib/net/ip.h>
#include <lib/net/ports.h>

#include <lib/util/buf.h>
#include <lib/util/eds.h>

struct frameconf {
  char *custom_frames;
  size_t custom_frameslen;
  const char *iface;
  unsigned int pps;
  unsigned int categories; /* CAT_ARP &c */
  char eth_src[ETH_ALEN];
  char eth_dst[ETH_ALEN];
  ip_addr_t src_ip;
  ip_addr_t curr_dst_ip;
  struct ip_blocks dst_ips;
  uint16_t curr_dst_port;
  struct port_ranges dst_ports;

  /* -- internal fields -- */
  size_t curr_buildix;
};

struct ethframe_client {
  buf_t buf;
  struct eth_sender *sender;
  struct frameconf cfg;

  unsigned int tpp; /* ticks per packet */
  unsigned int tppcount; /* ticks left until sending packets */
  unsigned int  npackets;
};

int ethframe_init(struct eds_service *svc);
void ethframe_fini(struct eds_service *svc);

void ethframe_on_readable(struct eds_client *cli, int fd);
void ethframe_on_done(struct eds_client *cli, int fd);

#endif /* ETHD_ETHFRAME_H__ */
