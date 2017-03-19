#include <stdio.h>
#include <stdlib.h>

#include <lib/net/eth.h>
#include <lib/net/ethframe.h>

int main() {
  eth_sender_t *sender;
  struct ethframe frame;

  static const struct ethframe_icmp4_ereq_opts opts_icmp4 = {
    .eth_dst = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    .eth_src = {0x00, 0x24, 0xd7, 0x17, 0x9c, 0x38},
    .ip_src = 0x1200a8c0, /* 192.168.0.18,  LSB order */
    .ip_dst = 0x010000e0, /* 224.0.0.1, LSB order */
    .ip_ttl = 1,
  };

  static const struct ethframe_icmp6_ereq_opts opts_icmp6 = {
    .eth_dst = {0x33, 0x33, 0x00, 0x00, 0x00, 0x01},
    .eth_src = {0x00, 0x24, 0xd7, 0x17, 0x9c, 0x38},
    .ip_src = {
        0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x24, 0xd7, 0xff, 0xfe, 0x17, 0x9c, 0x38
    },
    .ip_dst = {
        0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
    },
    .ip_hoplim = 64,
  };

  if ((sender = eth_sender_new("wlan0")) == NULL) {
    perror("eth_sender_new");
    return EXIT_FAILURE;
  }

  ethframe_icmp4_ereq_init(&frame, &opts_icmp4);
  eth_sender_send(sender, frame.buf, frame.len);
  ethframe_icmp6_ereq_init(&frame, &opts_icmp6);
  eth_sender_send(sender, frame.buf, frame.len);
  eth_sender_free(sender);
  return EXIT_SUCCESS;
}
