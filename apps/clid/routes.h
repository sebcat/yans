#ifndef CLID_ROUTES__H
#define CLID_ROUTES__H

#include <lib/util/eds.h>
#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>

struct routes_client {
  int flags;
  struct ycl_msg msgbuf;
  struct ycl_ctx ycl;

};

void routes_on_readable(struct eds_client *cli, int fd);
void routes_on_done(struct eds_client *cli, int fd);
void routes_on_finalize(struct eds_client *cli);

#endif
