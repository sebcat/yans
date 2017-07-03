#ifndef PROTO_STATUS_RESP_H__
#define PROTO_STATUS_RESP_H__

#include <proto/proto.h>

struct p_status_resp {
  const char *okmsg;
  size_t okmsglen;
  const char *errmsg;
  size_t errmsglen;
};

int p_status_resp_serialize(struct p_status_resp *data, buf_t *out);
int p_status_resp_deserialize(struct p_status_resp *data,
    char *in, size_t inlen, size_t *left);

#endif /* PROTO_STATUS_RESP_H__ */
