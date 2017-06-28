#ifndef PROTO_PCAP_REQ_H__
#define PROTO_PCAP_REQ_H__

#include <proto/proto.h>

struct p_pcap_req {
  const char *iface;
  size_t ifacelen;
  const char *filter;
  size_t filterlen;
};

int p_pcap_req_serialize(struct p_pcap_req *data, buf_t *out);
int p_pcap_req_deserialize(struct p_pcap_req *data, char *in, size_t inlen, size_t *left);

#endif /* PROTO_PCAP_REQ_H__ */
