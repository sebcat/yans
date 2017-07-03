#include <string.h>

#include <proto/status_resp.h>

static struct netstring_map status_resp_m[] = {
  NETSTRING_MENTRY(struct p_status_resp, okmsg),
  NETSTRING_MENTRY(struct p_status_resp, errmsg),
  NETSTRING_MENTRY_END,
};
int p_status_resp_serialize(struct p_status_resp *data, buf_t *out) {
  return netstring_serialize(data, status_resp_m, out);
}

int p_status_resp_deserialize(struct p_status_resp *data,
    char *in, size_t inlen, size_t *left) {
  memset(data, 0, sizeof(struct p_status_resp));
  return netstring_deserialize(data, status_resp_m, in, inlen, left);
}
