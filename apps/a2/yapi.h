#ifndef YANS_YAPI_H__
#define YANS_YAPI_H__

#include <setjmp.h>

enum yapi_status {
  YAPI_STATUS_NONE = 0,
  YAPI_STATUS_OK,
  YAPI_STATUS_BAD_REQUEST,
  YAPI_STATUS_NOT_FOUND,
  YAPI_STATUS_INTERNAL_SERVER_ERROR,
};

enum yapi_ctype {
  YAPI_CTYPE_NONE = 0,
  YAPI_CTYPE_JSON,
  YAPI_CTYPE_CSV,
  YAPI_CTYPE_TEXT,
  YAPI_CTYPE_BINARY,
};

enum yapi_method {
  YAPI_METHOD_UNKNOWN = 0,
  YAPI_METHOD_GET,
  YAPI_METHOD_POST,
};

struct yapi_request {
  size_t      content_length;
  const char *content_type;
  const char *document_uri;
  const char *query_string;
  enum yapi_method request_method;
};

struct yapi_response {
  enum yapi_status status;
  enum yapi_ctype content_type;
};

struct yapi_ctx {
  struct yapi_request req;
  struct yapi_response resp;
  jmp_buf jmpbuf;
  int input;
  int output;
};

struct yapi_route {
  enum yapi_method method;
  const char *path;
  int (*func)(struct yapi_ctx *);
};

const char *yapi_ctype2str(enum yapi_ctype t);
const char *yapi_method2str(enum yapi_method method);
const char *yapi_status2str(enum yapi_status status);
enum yapi_method yapi_str2method(const char *str);

void yapi_init(struct yapi_ctx *rutt);

static inline void yapi_set_input(struct yapi_ctx *rutt, int fd) {
  rutt->input = fd;
}

static inline void yapi_set_output(struct yapi_ctx *rutt, int fd) {
  rutt->output = fd;
}

int yapi_serve(struct yapi_ctx *rutt, const char *prefix,
    struct yapi_route *routes, size_t nroutes);

#endif
