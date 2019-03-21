#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <lib/net/scgi.h>
#include <apps/a2/rutt.h>

const char *rutt_ctype2str(enum rutt_ctype t) {
  switch(t) {
  case RUTT_CTYPE_JSON:
    return "application/json";
  case RUTT_CTYPE_CSV:
    return "application/vnd.ms-excel";
  case RUTT_CTYPE_TEXT:
    return "text/plain";
  case RUTT_CTYPE_BINARY:
  default:
    return "application/octet-stream";
    break;
  }
}

const char *rutt_method2str(enum rutt_method method) {
  switch (method) {
    case RUTT_METHOD_GET:
      return "GET";
    case RUTT_METHOD_POST:
      return "POST";
    case RUTT_METHOD_UNKNOWN:
      return "DUNNO";
  }
}

const char *rutt_status2str(enum rutt_status status) {
  switch (status) {
  case RUTT_STATUS_OK:
    return "200 OK";
  case RUTT_STATUS_BAD_REQUEST:
    return "400 Bad Request";
  case RUTT_STATUS_NOT_FOUND:
    return "404 Not Found";
  case RUTT_STATUS_INTERNAL_SERVER_ERROR:
    return "500 Internal Server Error";
  default:
    return "666 The Number Of The Beast";  
  }
}

enum rutt_method rutt_str2method(const char *str) {
  if (strcmp(str, "GET") == 0) {
    return RUTT_METHOD_GET;
  } else if (strcmp(str, "POST") == 0) {
    return RUTT_METHOD_POST;
  } else {
    return RUTT_METHOD_UNKNOWN;  
  }
}

static int parse_request_header(struct scgi_ctx *cgi,
    struct rutt_request *req) {
  struct scgi_header hdr = {0};
  int ret;
  long l;

  /* Parse the actual SCGI header */
  ret = scgi_parse_header(cgi);
  if (ret != SCGI_OK) {
    return ret;
  }

  /* Iterate over the request header fields and store appropriate fields in
   * rutt_request. We should only check for the things we care about. */
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
        req->request_method = rutt_str2method(hdr.value);
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

static int route_request(struct rutt_ctx *ctx, const char *prefix,
    struct rutt_route *routes, size_t nroutes) {
  const char *path;
  size_t len;
  size_t i;
  struct rutt_route *route;
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

  /* Check if a matching route was found. If not, return. */
  if (i == nroutes) {
    return 0;
  }

  ret = setjmp(ctx->jmpbuf);
  if (ret) {
    goto handle_err;
  }

  return route->func(ctx);

handle_err:
  /* TODO: Implement */
  printf(
      "Status: %s\r\n"
      "Content-Type: %s\r\n\r\n"
      "%s"
      , "666", "some/type", "lol");
  return ret;
}

void rutt_init(struct rutt_ctx *rutt) {
  memset(rutt, 0, sizeof(*rutt));
}

int rutt_serve(struct rutt_ctx *rutt, const char *prefix,
    struct rutt_route *routes, size_t nroutes) {
  struct scgi_ctx cgi = {0};
  int status = EXIT_FAILURE;
  int ret;

  ret = scgi_init(&cgi, rutt->input, SCGI_DEFAULT_MAXHDRSZ);
  if (ret != SCGI_OK) {
    goto end;
  }

  while ((ret = scgi_read_header(&cgi)) == SCGI_AGAIN);
  if (ret != SCGI_OK) {
    goto scgi_cleanup;
  }

  ret = parse_request_header(&cgi, &rutt->req);
  if (ret != SCGI_OK) {
    goto scgi_cleanup;
  }

  /* TODO:
   *  - scgi_get_rest must be used when reading request bodies
   *  - should scgi, stdio should be wrapped by rutt?
   *  - return rutt_error(...), similar to Lua, using longjmp.
   **/

  status = route_request(rutt, prefix, routes, nroutes);
scgi_cleanup:
  scgi_cleanup(&cgi);
end:
  return status;
}

