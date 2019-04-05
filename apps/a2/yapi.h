#ifndef YANS_YAPI_H__
#define YANS_YAPI_H__

#include <setjmp.h>
#include <stdio.h>

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
  size_t content_length;
  char *content_type;
  char *document_uri;
  char *query_string;
  char *accept_encoding;
  enum yapi_method request_method;
};

struct yapi_ctx {
  struct yapi_request req;
  jmp_buf jmpbuf;
  size_t maxlen;
  FILE *input;
  FILE *output;
  void *data;
  const char *rest;
  size_t restlen;
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

void yapi_init(struct yapi_ctx *ctx);

static inline void yapi_set_data(struct yapi_ctx *ctx, void *data) {
  ctx->data = data;
}

static inline void *yapi_data(struct yapi_ctx *ctx) {
  return ctx->data;
}

int yapi_header(struct yapi_ctx *ctx, enum yapi_status status,
    enum yapi_ctype ctype);
int yapi_headers(struct yapi_ctx *ctx, enum yapi_status status,
    enum yapi_ctype ctype, ...);
int yapi_write(struct yapi_ctx *ctx, const void *data, size_t len);
int yapi_writef(struct yapi_ctx *ctx, const char *fmt, ...);
int yapi_read(struct yapi_ctx *ctx, buf_t *dst);

#define yapi_errorf(ctx__, status__, fmt__, ...) \
  _yapi_errorf((ctx__), (status__), "[%s:%d] " fmt__ "\n", __func__, \
      __LINE__, __VA_ARGS__)

#define yapi_error(ctx__, status__, str__) \
  _yapi_errorf((ctx__), (status__), "[%s:%d] %s\n", __func__, __LINE__, \
      (str__))

int _yapi_errorf(struct yapi_ctx *ctx, enum yapi_status status,
    const char *fmt, ...);


int yapi_serve(struct yapi_ctx *ctx, const char *prefix,
    struct yapi_route *routes, size_t nroutes);

#endif
