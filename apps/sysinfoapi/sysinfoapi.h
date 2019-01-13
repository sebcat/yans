#ifndef YANS_SYSINFOAPI_H__
#define YANS_SYSINFOAPI_H__

#include <lib/util/eds.h>

#define SYSINFOAPI_CLI(cli__) \
    (struct sysinfoapi_cli*)((cli__)->udata)

struct sysinfoapi_cli {
  char body[128];
  int nbytes_body;
  int offset;
};

void sysinfoapi_set_rootpath(const char *root);
void sysinfoapi_on_writable(struct eds_client *cli, int fd);

#endif
