#ifndef KNEGD_KNG_H__
#define KNEGD_KNG_H__

#include <lib/ycl/ycl.h>
#include <lib/util/eds.h>

#define KNG_CLI(cli__) \
    (struct kng_cli*)((cli__)->udata)

struct kng_cli {
  int flags;
  struct ycl_ctx ycl;
  struct ycl_msg msgbuf;
};

void kng_on_readable(struct eds_client *cli, int fd);
void kng_on_done(struct eds_client *cli, int fd);
void kng_on_svc_reaped_child(struct eds_service *svc, pid_t pid,
    int status);
void kng_on_finalize(struct eds_client *cli);
void kng_mod_fini(struct eds_service *svc);

#endif
