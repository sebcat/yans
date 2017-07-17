#ifndef PROTO_SWEEPER_REQ_H__
#define PROTO_SWEEPER_REQ_H__

#include <proto/proto.h>

struct p_sweeper_req {
  const char *addrs;
  size_t addrslen;
  const char *arp;
  size_t arplen;
};

int p_sweeper_req_serialize(struct p_sweeper_req *data, buf_t *out);
int p_sweeper_req_deserialize(struct p_sweeper_req *data,
    char *in, size_t inlen, size_t *left);

#endif /* PROTO_SWEEPER_REQ_H__ */
