#ifndef MKREPORT_REPORT_H__
#define MKREPORT_REPORT_H__

struct report_opts {
  const char *output;
  char **inputs;
  int ninputs;
};

struct report_types {
  const char *name;
  int (*func)(struct report_opts *);
};

#endif
