#ifndef PROTO_ETHFRAME_REQ_H__
#define PROTO_ETHFRAME_REQ_H__

#include <proto/proto.h>

struct p_ethframe_req {
  const char *custom_frames;
  size_t custom_frameslen;
  const char *categories;
  size_t categorieslen;
  const char *pps;
  size_t ppslen;
  const char *iface;
  size_t ifacelen;
  const char *eth_src;
  size_t eth_srclen;
  const char *eth_dst;
  size_t eth_dstlen;
  const char *ip_src;
  size_t ip_srclen;
  const char *ip_dsts;
  size_t ip_dstslen;
  const char *port_dsts;
  size_t port_dstslen;
};

int p_ethframe_req_serialize(struct p_ethframe_req *data, buf_t *out);
int p_ethframe_req_deserialize(struct p_ethframe_req *data,
    char *in, size_t inlen, size_t *left);

#endif /* PROTO_ETHFRAME_REQ_H__ */
