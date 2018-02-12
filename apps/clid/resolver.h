#ifndef YANS_RESOLVER_H__
#define YANS_RESOLVER_H__

#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>
#include <lib/util/eds.h>

struct resolver_cli {
  int flags;
  struct ycl_ctx ycl;
  struct ycl_msg msgbuf;
  FILE *resfile; /* the result will be written here, received from client */
  int closefds[2]; /* socketpair fds to signal complete */
  const char *hosts; /* pointer into parsed msgbuf containing hosts */
};

/* must be called before resolver_init */
void resolver_set_nresolvers(unsigned short nresolvers);
int resolver_init(struct eds_service *svc);
void resolver_on_readable(struct eds_client *cli, int fd);
void resolver_on_done(struct eds_client *cli, int fd);

#endif
