#ifndef YANS_SCAND_H__
#define YANS_SCAND_H__

#include <lib/ycl/ycl.h>
#include <lib/util/eds.h>

#define SCANNER_CLI(cli__) \
    (struct scanner_cli*)((cli__)->udata)

struct scanner_cli {
  int flags;
  struct ycl_ctx ycl;
  struct ycl_msg msgbuf;
};

void scanner_on_readable(struct eds_client *cli, int fd);
void scanner_on_done(struct eds_client *cli, int fd);
void scanner_on_svc_reaped_child(struct eds_service *svc, pid_t pid,
    int status);
void scanner_on_finalize(struct eds_client *cli);
void scanner_mod_fini(struct eds_service *svc);

#endif
