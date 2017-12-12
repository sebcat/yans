#ifndef CLID_STORE_H__
#define CLID_STORE_H__

#include <lib/ycl/ycl.h>
#include <lib/util/eds.h>

#define STORE_CLI(cli__) \
    (struct store_cli*)((cli__)->udata)

struct store_cli {
  int flags;
  struct ycl_ctx ycl;
  struct ycl_msg msgbuf;

};

void store_on_readable(struct eds_client *cli, int fd);
void store_on_done(struct eds_client *cli, int fd);
void store_on_finalize(struct eds_client *cli);

#endif
