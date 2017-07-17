#include <string.h>

#include <proto/sweeper_req.h>

static struct netstring_map sweeper_req_m[] = {
  NETSTRING_MENTRY(struct p_sweeper_req, addrs),
  NETSTRING_MENTRY(struct p_sweeper_req, arp),
  NETSTRING_MENTRY_END,
};
int p_sweeper_req_serialize(struct p_sweeper_req *data, buf_t *out) {
  return netstring_serialize(data, sweeper_req_m, out);
}

int p_sweeper_req_deserialize(struct p_sweeper_req *data,
    char *in, size_t inlen, size_t *left) {
  memset(data, 0, sizeof(struct p_sweeper_req));
  return netstring_deserialize(data, sweeper_req_m, in, inlen, left);
}
