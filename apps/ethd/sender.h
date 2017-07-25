#ifndef ETHD_SENDER_H__
#define ETHD_SENDER_H__

#include <lib/util/buf.h>
#include <lib/util/eds.h>

struct sender_client {
  buf_t buf;
};

int sender_init(struct eds_service *svc);
void sender_fini(struct eds_service *svc);
void sender_on_readable(struct eds_client *cli, int fd);
void sender_on_done(struct eds_client *cli, int fd);

#endif
