#ifndef PROTO_ETHFRAME_REQ_H__
#define PROTO_ETHFRAME_REQ_H__

#include <proto/proto.h>

struct p_ethframe_req {
  const char *iface;
  size_t ifacelen;
  const char *frames;
  size_t frameslen;
};

int p_ethframe_req_serialize(struct p_ethframe_req *data, buf_t *out);
int p_ethframe_req_deserialize(struct p_ethframe_req *data,
    char *in, size_t inlen, size_t *left);

#endif /* PROTO_ETHFRAME_REQ_H__ */
