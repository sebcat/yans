#include <string.h>

#include <proto/pcap_req.h>

static struct netstring_map pcap_req_m[] = {
  NETSTRING_MENTRY(struct p_pcap_req, iface),
  NETSTRING_MENTRY(struct p_pcap_req, filter),
  NETSTRING_MENTRY_END,
};
int p_pcap_req_serialize(struct p_pcap_req *data, buf_t *out) {
  return netstring_serialize(data, pcap_req_m, out);
}

int p_pcap_req_deserialize(struct p_pcap_req *data, char *in, size_t inlen) {
  memset(data, 0, sizeof(struct p_pcap_req));
  return netstring_deserialize(data, pcap_req_m, in, inlen);
}
