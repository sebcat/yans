#ifndef YANS_RUTT_H__
#define YANS_RUTT_H__

#include <setjmp.h>

enum rutt_status {
  RUTT_STATUS_NONE = 0,
  RUTT_STATUS_OK,
  RUTT_STATUS_BAD_REQUEST,
  RUTT_STATUS_NOT_FOUND,
  RUTT_STATUS_INTERNAL_SERVER_ERROR,
};

enum rutt_ctype {
  RUTT_CTYPE_NONE = 0,
  RUTT_CTYPE_JSON,
  RUTT_CTYPE_CSV,
  RUTT_CTYPE_TEXT,
  RUTT_CTYPE_BINARY,
};

enum rutt_method {
  RUTT_METHOD_UNKNOWN = 0,
  RUTT_METHOD_GET,
  RUTT_METHOD_POST,
};

struct rutt_request {
  size_t      content_length;
  const char *content_type;
  const char *document_uri;
  const char *query_string;
  enum rutt_method request_method;
};

struct rutt_response {
  enum rutt_status status;
  enum rutt_ctype content_type;
};

struct rutt_ctx {
  struct rutt_request req;
  struct rutt_response resp;
  jmp_buf jmpbuf;
  int input;
  int output;
};

struct rutt_route {
  enum rutt_method method;
  const char *path;
  int (*func)(struct rutt_ctx *);
};

const char *rutt_ctype2str(enum rutt_ctype t);
const char *rutt_method2str(enum rutt_method method);
const char *rutt_status2str(enum rutt_status status);
enum rutt_method rutt_str2method(const char *str);

void rutt_init(struct rutt_ctx *rutt);

static inline void rutt_set_input(struct rutt_ctx *rutt, int fd) {
  rutt->input = fd;
}

static inline void rutt_set_output(struct rutt_ctx *rutt, int fd) {
  rutt->output = fd;
}

int rutt_serve(struct rutt_ctx *rutt, const char *prefix,
    struct rutt_route *routes, size_t nroutes);

#endif
