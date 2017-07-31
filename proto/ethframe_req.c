#include <string.h>

#include <proto/ethframe_req.h>

static struct netstring_map ethframe_req_m[] = {
  NETSTRING_MENTRY(struct p_ethframe_req, iface),
  NETSTRING_MENTRY(struct p_ethframe_req, frames),
  NETSTRING_MENTRY(struct p_ethframe_req, arpreq_addrs),
  NETSTRING_MENTRY(struct p_ethframe_req, arpreq_sha),
  NETSTRING_MENTRY(struct p_ethframe_req, arpreq_spa),
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
