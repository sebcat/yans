#ifndef PROTO_ETHFRAME_REQ_H__
#define PROTO_ETHFRAME_REQ_H__

#include <proto/proto.h>

struct p_ethframe_req {
  const char *iface;
  size_t ifacelen;
  const char *frames;
  size_t frameslen;
  const char *arpreq_addrs;
  size_t arpreq_addrslen;
  const char *arpreq_sha;
  size_t arpreq_shalen;
  const char *arpreq_spa;
  size_t arpreq_spalen;
};

int p_ethframe_req_serialize(struct p_ethframe_req *data, buf_t *out);
int p_ethframe_req_deserialize(struct p_ethframe_req *data,
    char *in, size_t inlen, size_t *left);

#endif /* PROTO_ETHFRAME_REQ_H__ */
