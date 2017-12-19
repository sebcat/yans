#ifndef CLID_STORE_H__
#define CLID_STORE_H__

#include <lib/ycl/ycl.h>
#include <lib/util/eds.h>

#define STORE_CLI(cli__) \
    (struct store_cli*)((cli__)->udata)

/* limits */
#define STORE_IDSZ         20
#define STORE_PREFIXSZ      2 /* must be smaller than STORE_IDSZ */
#define STORE_MAXPATH     128
#define STORE_MAXDIRPATH  \
    STORE_PREFIXSZ + 1 + STORE_IDSZ + 1 + STORE_MAXPATH + 1

#define STORE_ID(ecli) \
    ((ecli)->store_path + STORE_PREFIXSZ + 1)

struct store_cli {
  int flags;
  struct ycl_ctx ycl;
  struct ycl_msg msgbuf;
  char store_path[STORE_IDSZ + STORE_PREFIXSZ + 2]; /* "%s/%s" */
  int open_fd;
  int open_errno;
};

int store_init(struct eds_service *svc);
void store_on_readable(struct eds_client *cli, int fd);
void store_on_done(struct eds_client *cli, int fd);
void store_on_finalize(struct eds_client *cli);

#endif
