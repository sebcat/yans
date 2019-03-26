#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <lib/net/scgi.h>
#include <apps/a2/yapi.h>

const char *yapi_ctype2str(enum yapi_ctype t) {
  switch(t) {
  case YAPI_CTYPE_JSON:
    return "application/json";
  case YAPI_CTYPE_CSV:
    return "application/vnd.ms-excel";
  case YAPI_CTYPE_TEXT:
    return "text/plain;charset=utf-8";
  case YAPI_CTYPE_BINARY:
  default:
    return "application/octet-stream";
    break;
  }
}

const char *yapi_method2str(enum yapi_method method) {
  switch (method) {
    case YAPI_METHOD_GET:
      return "GET";
    case YAPI_METHOD_POST:
      return "POST";
    case YAPI_METHOD_UNKNOWN:
    default:
      return "DUNNO";
  }
}

const char *yapi_status2str(enum yapi_status status) {
  switch (status) {
  case YAPI_STATUS_OK:
    return "200 OK";
  case YAPI_STATUS_BAD_REQUEST:
    return "400 Bad Request";
  case YAPI_STATUS_NOT_FOUND:
    return "404 Not Found";
  case YAPI_STATUS_INTERNAL_SERVER_ERROR:
    return "500 Internal Server Error";
  default:
    return "666 The Number Of The Beast";  
  }
}

enum yapi_method yapi_str2method(const char *str) {
  if (strcmp(str, "GET") == 0) {
    return YAPI_METHOD_GET;
  } else if (strcmp(str, "POST") == 0) {
    return YAPI_METHOD_POST;
  } else {
    return YAPI_METHOD_UNKNOWN;  
  }
}

static int parse_request_header(struct scgi_ctx *cgi,
    struct yapi_request *req) {
  struct scgi_header hdr = {0};
  int ret;
  long l;

  /* Parse the actual SCGI header */
  ret = scgi_parse_header(cgi);
  if (ret != SCGI_OK) {
    return ret;
  }

  /* Iterate over the request header fields and store appropriate fields in
   * yapi_request. We should only check for the things we care about. */
  while ((ret = scgi_get_next_header(cgi, &hdr)) == SCGI_AGAIN) {

    /* Skip headers with empty values for now */
    if (hdr.valuelen == 0) {
      continue;
    }

    /* Switch on the first character of the key. No need to match entire
     * strings at this point - we probably don't care about most of them */
    switch (hdr.key[0]) {
    case 'C':
      if (strcmp(hdr.key, "CONTENT_TYPE") == 0) {
        req->content_type = hdr.value;
      } else if (strcmp(hdr.key, "CONTENT_LENGTH") == 0) {
        l = strtol(hdr.value, NULL, 10);
        if (l > 0) {
          req->content_length = (size_t)l;
        }
      }
      break;
    case 'R':
      if (strcmp(hdr.key, "REQUEST_METHOD") == 0) {
        req->request_method = yapi_str2method(hdr.value);
      }
      break;
    case 'D':
      if (strcmp(hdr.key, "DOCUMENT_URI") == 0) {
        req->document_uri = hdr.value;
      }
      break;
    case 'Q':
      if (strcmp(hdr.key, "QUERY_STRING") == 0) {
        req->query_string = hdr.value;
      }
      break;
    }
  }

  return ret;
}

static int route_request(struct yapi_ctx *ctx, const char *prefix,
    struct yapi_route *routes, size_t nroutes) {
  const char *path;
  size_t len;
  size_t i;
  struct yapi_route *route = NULL;
  int ret;

  path = ctx->req.document_uri;
  if (path == NULL || *path == '\0') {
    return 0;
  }

  /* Check if the API prefix matches */
  if (prefix) {
    len = strlen(prefix);
    if (strncmp(prefix, path, len) != 0) {
      /* Not the expected prefix means no match */
      return 0;
    }

    /* skip past the path prefix */
    path += len;
  }

  /* iterate over the routes, break on match. O(n) where n assumed to be
   * small */
  for (i = 0; i < nroutes; i++) {
    route = &routes[i];

    if (route->method == ctx->req.request_method &&
        strcmp(route->path, path) == 0) {
      break;
    }
  }

  /* Check if a matching route was found. If not, respond with 404. */
  if (i == nroutes) {
    yapi_header(ctx, YAPI_STATUS_NOT_FOUND, YAPI_CTYPE_TEXT);
    yapi_write(ctx, "404 not found\n", sizeof("404 not found\n")-1);
    return 0;
  }

  ret = setjmp(ctx->jmpbuf);
  if (ret) {
    /* _yapi_errorf was called from within the route func, bail with the
     * return code supplied from _yapi_errorf */
    return ret;
  }

  return route->func(ctx);
}

int yapi_header(struct yapi_ctx *ctx, enum yapi_status status,
    enum yapi_ctype ctype) {
  return fprintf(ctx->output,
      "Status: %s\r\n"
      "Content-Type: %s\r\n\r\n",
      yapi_status2str(status),
      yapi_ctype2str(ctype));
}

int yapi_write(struct yapi_ctx *ctx, const void *data, size_t len) {
  return fwrite(data, 1, len, ctx->output);
}

int yapi_writef(struct yapi_ctx *ctx, const char *fmt, ...) {
  va_list ap;
  int ret;

  va_start(ap, fmt);
  ret = vfprintf(ctx->output, fmt, ap);
  va_end(ap);
  return ret;
}

int _yapi_errorf(struct yapi_ctx *ctx, enum yapi_status status,
    const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  yapi_header(ctx, status, YAPI_CTYPE_TEXT);
  vfprintf(ctx->output, fmt, ap);
  va_end(ap);
  longjmp(ctx->jmpbuf, 1);
  return 1; /* not reached */
}

void yapi_init(struct yapi_ctx *ctx) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->input  = stdin;
  ctx->output = stdout;
}

int yapi_serve(struct yapi_ctx *ctx, const char *prefix,
    struct yapi_route *routes, size_t nroutes) {
  struct scgi_ctx cgi = {0};
  int status = EXIT_FAILURE;
  int ret;

  ret = scgi_init(&cgi, fileno(ctx->input), SCGI_DEFAULT_MAXHDRSZ);
  if (ret != SCGI_OK) {
    goto end;
  }

  while ((ret = scgi_read_header(&cgi)) == SCGI_AGAIN);
  if (ret != SCGI_OK) {
    goto scgi_cleanup;
  }

  ret = parse_request_header(&cgi, &ctx->req);
  if (ret != SCGI_OK) {
    goto scgi_cleanup;
  }

  /* TODO:
   *  - scgi_get_rest must be used when reading request bodies
   **/

  status = route_request(ctx, prefix, routes, nroutes);
scgi_cleanup:
  scgi_cleanup(&cgi);
end:
  return status;
}

