#ifndef CLID_FILE_H__
#define CLID_FILE_H__

#include <lib/util/eds.h>

struct file_cli {

};

void file_on_readable(struct eds_client *cli, int fd);
void file_on_done(struct eds_client *cli, int fd);

#endif
