#ifndef ETHD_ETHFRAME_H__
#define ETHD_ETHFRAME_H__

#include <lib/net/eth.h>

#include <lib/util/buf.h>
#include <lib/util/eds.h>

struct ethframe_client {
  buf_t buf;
};

eds_action_result ethframe_on_readable(struct eds_client *cli, int fd);
void ethframe_on_done(struct eds_client *cli, int fd);

#endif /* ETHD_ETHFRAME_H__ */
