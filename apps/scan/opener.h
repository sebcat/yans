#ifndef SCAN_OPENER_H__
#define SCAN_OPENER_H__

#include <lib/ycl/ycl.h>

#define OPENERF_COMPRESS_IN  (1 << 0)
#define OPENERF_COMPRESS_OUT (1 << 0)

struct opener_opts {
  struct ycl_msg *msgbuf;
  const char *socket;
  const char *store_id;
};

int opener_init(struct opener_opts *opts);
void opener_cleanup();
int opener_fopen(const char *path, const char *mode, int flags, FILE **fp);
const char *opener_strerr();

#endif
