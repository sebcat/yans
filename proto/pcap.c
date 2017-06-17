#include <string.h>

#include <proto/pcap.h>

static struct netstring_map pcap_cmd_m[] = {
  NETSTRING_MENTRY(struct p_pcap_cmd, iface),
  NETSTRING_MENTRY(struct p_pcap_cmd, filter),
  NETSTRING_MENTRY_END,
};

int p_pcap_cmd_serialize(struct p_pcap_cmd *data, buf_t *out) {
  return netstring_serialize(data, pcap_cmd_m, out);
}

int p_pcap_cmd_deserialize(struct p_pcap_cmd *data, char *in, size_t inlen) {
  memset(data, 0, sizeof(struct p_pcap_cmd));
  return netstring_deserialize(data, pcap_cmd_m, in, inlen);
}
