#include <string.h>

#include <proto/ethframe_req.h>

static struct netstring_map ethframe_req_m[] = {
  NETSTRING_MENTRY(struct p_ethframe_req, custom_frames),
  NETSTRING_MENTRY(struct p_ethframe_req, categories),
  NETSTRING_MENTRY(struct p_ethframe_req, pps),
  NETSTRING_MENTRY(struct p_ethframe_req, iface),
  NETSTRING_MENTRY(struct p_ethframe_req, eth_src),
  NETSTRING_MENTRY(struct p_ethframe_req, eth_dst),
  NETSTRING_MENTRY(struct p_ethframe_req, ip_src),
  NETSTRING_MENTRY(struct p_ethframe_req, ip_dsts),
  NETSTRING_MENTRY(struct p_ethframe_req, port_dsts),
  NETSTRING_MENTRY_END,
};
int p_ethframe_req_serialize(struct p_ethframe_req *data, buf_t *out) {
  return netstring_serialize(data, ethframe_req_m, out);
}

int p_ethframe_req_deserialize(struct p_ethframe_req *data,
    char *in, size_t inlen, size_t *left) {
  memset(data, 0, sizeof(struct p_ethframe_req));
  return netstring_deserialize(data, ethframe_req_m, in, inlen, left);
}
