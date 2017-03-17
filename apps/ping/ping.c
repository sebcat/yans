#include <stdio.h>
#include <stdlib.h>

#include <lib/net/eth.h>
#include <lib/net/etherframe.h>

int main() {
  eth_sender_t *sender;
  struct ethframe frame;
  static const struct ethframe_icmp4_opts opts = {
    .eth_dst = {0xdc, 0x53, 0x7c, 0x28, 0xa6, 0xc5},
    .eth_src = {0x00, 0x24, 0xd7, 0x17, 0x9c, 0x38},
    .ip_src = 0x1200a8c0, /* 192.168.0.18,  LSB order */
    .ip_dst = 0x08080808,
  };

  if ((sender = eth_sender_new("wlan0")) == NULL) {
    perror("eth_sender_new");
    return EXIT_FAILURE;
  }

  ethframe_icmp4_init(&frame, &opts);
  eth_sender_send(sender, frame.buf, frame.len);
  eth_sender_free(sender);
  return EXIT_SUCCESS;
}
