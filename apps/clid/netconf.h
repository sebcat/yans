#ifndef CLID_NETCONF__H
#define CLID_NETCONF__H

#include <lib/util/eds.h>
#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>

struct netconf_client {
  int flags;
  struct ycl_msg msgbuf;
  struct ycl_ctx ycl;

};

void netconf_on_readable(struct eds_client *cli, int fd);
void netconf_on_done(struct eds_client *cli, int fd);
void netconf_on_finalize(struct eds_client *cli);

#endif
