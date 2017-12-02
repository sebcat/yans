#ifndef YCLGEN_MSGS_H__
#define YCLGEN_MSGS_H__
#include <stddef.h>

#include <lib/ycl/ycl.h>

struct ycl_data {
  const char *data;
  size_t len;
};

struct ycl_msg_baz {
  struct ycl_data d;
  const char *s;
  long l;
  struct ycl_data *darr;
  size_t ndarr;
  const char **sarr;
  size_t nsarr;
  long *larr;
  size_t nlarr;
};

int ycl_msg_parse_baz(struct ycl_msg *msg, struct ycl_msg_baz *r);
int ycl_msg_create_baz(struct ycl_msg *msg, struct ycl_msg_baz *r);

