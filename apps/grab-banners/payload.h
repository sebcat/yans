#ifndef BGRAB_PAYLOAD_H__
#define BGRAB_PAYLOAD_H__

#include <sys/types.h>
#include <sys/socket.h>

#include <lib/util/buf.h>

struct payload_data {
  buf_t buf;
};

struct payload_host {
  struct sockaddr *sa;
  socklen_t salen;
  const char *name;
};

struct payload_data *payload_build(const char *fmt, ...);
void payload_free(struct payload_data *payload);

static inline void *payload_get_data(struct payload_data *payload) {
  return payload->buf.data;
}

static inline size_t payload_get_len(struct payload_data *payload) {
  return payload->buf.len;
}

#endif
