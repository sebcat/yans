#ifndef ETHD_ETHFRAME_H__
#define ETHD_ETHFRAME_H__

#include <stdint.h>

#include <lib/net/eth.h>
#include <lib/net/ip.h>

#include <lib/util/buf.h>
#include <lib/util/eds.h>

struct ethframe_client {
  buf_t buf;
  struct eth_sender *sender;

  /* arpreq addrs, SPA and SHA */
  struct ip_blocks addrs;
  uint32_t spa;
  char sha[6];
};

int ethframe_init(struct eds_service *svc);
void ethframe_fini(struct eds_service *svc);

void ethframe_on_readable(struct eds_client *cli, int fd);
void ethframe_on_done(struct eds_client *cli, int fd);

#endif /* ETHD_ETHFRAME_H__ */
