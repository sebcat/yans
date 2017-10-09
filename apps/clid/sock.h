#ifndef CLID_SOCK_H__
#define CLID_SOCK_H__

#include <lib/util/eds.h>

struct sock_cli {

};

void sock_on_readable(struct eds_client *eds, int fd);
void sock_on_done(struct eds_client *eds, int fd);

#endif

