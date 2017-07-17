#ifndef ETHD_SWEEPER_H__
#define ETHD_SWEEPER_H__

#include <lib/util/buf.h>
#include <lib/util/eds.h>

struct sweeper_client {
  buf_t buf;

};

void sweeper_on_readable(struct eds_client *cli, int fd);
void sweeper_on_done(struct eds_client *cli, int fd);

#endif
