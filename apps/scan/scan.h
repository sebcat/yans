#ifndef YANS_SCAN_H__
#define YANS_SCAN_H__

#include <lib/ycl/ycl.h>
#include <lib/ycl/opener.h>

struct scan_ctx {
  struct ycl_msg msgbuf;
  struct opener_ctx opener;
};

struct scan_cmd {
  const char *name;
  int (*func)(struct scan_ctx *, int, char **);
  const char *desc;
};

#endif
