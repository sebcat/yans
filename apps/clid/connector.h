#ifndef YANS_CONNECTOR_H__
#define YANS_CONNECTOR_H__

#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>

struct connector_cli {
  int flags;
  struct ycl_ctx ycl;
  struct ycl_msg msgbuf;
  int connfd;
  int connerr;
};


void connector_on_readable(struct eds_client *cli, int fd);
void connector_on_done(struct eds_client *cli, int fd);
void connector_on_finalize(struct eds_client *cli);

#endif
