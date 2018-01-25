#ifndef KNEGD_KNG_H__
#define KNEGD_KNG_H__

#include <lib/ycl/ycl.h>
#include <lib/util/eds.h>

#define KNG_CLI(cli__) \
    (struct kng_cli*)((cli__)->udata)

#ifndef DATAROOTDIR
#define DATAROOTDIR "/usr/local/share"
#endif

#ifndef DFL_KNEGDIR
#define DFL_KNEGDIR DATAROOTDIR "/kneg"
#endif

struct kng_cli {
  int flags;
  struct ycl_ctx ycl;
  struct ycl_msg msgbuf;
};

void kng_set_knegdir(const char *dir);
void kng_on_readable(struct eds_client *cli, int fd);
void kng_on_svc_reaped_child(struct eds_service *svc, pid_t pid,
    int status);
void kng_on_finalize(struct eds_client *cli);
void kng_mod_fini(struct eds_service *svc);

#endif
