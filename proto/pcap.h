#ifndef PROTO_PCAP_H__
#define PROTO_PCAP_H__

#include <proto/proto.h>

struct p_pcap_cmd {
  const char *iface;
  const char *filter;
  size_t ifacelen;
  size_t filterlen;
};

int p_pcap_cmd_serialize(struct p_pcap_cmd *data, buf_t *out);
int p_pcap_cmd_deserialize(struct p_pcap_cmd *data, char *in, size_t inlen);

#endif /* PROTO_PCAP_H__ */
