/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <jansson.h>

#include <lib/ycl/yclcli_kneg.h>
#include <lib/ycl/yclcli_store.h>

#include <lib/util/sindex.h>
#include <lib/util/zfile.h>
#include <lib/util/macros.h>
#include <lib/util/sc2mod.h>
#include <lib/util/u8.h>
#include <lib/net/urlquery.h>
#include <apps/a2/yapi.h>

#define DFL_KNEG_TYPE "scan"

struct a2_ctx {
  struct ycl_msg msgbuf;
  struct yclcli_ctx kneg;
  struct yclcli_ctx store;
};

struct a2_ctx ctx_; /* should only be accessed by sc2_setup if we want to
                     * allocate it dynamically in the future */

static int is_valid_id(const char *id) {
  size_t i;

  if (id == NULL) {
    return 0;
  }

  for (i = 0; i < 20; i++) {
    if (!strchr("0123456789abcdef", id[i])) {
      return 0;
    }
  }

  if (id[i] != '\0') {
    return 0;
  }

  return 1;
}

static int get_fail(struct yapi_ctx *ctx) {
  return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR, "got fail");
}

/*
 * GET /a1/queueinfo
 *
 * Response object on success:
 * {
 *   "success": true,
 *   "data": {
 *     "nrunning": 0,
 *     "nslots": 0,
 *     "last-waited-for": 0
 *   }
 * }
 */
static int get_queueinfo(struct yapi_ctx *ctx) {
  struct a2_ctx *a2data;
  int ret;
  json_t *data;
  json_t *top;
  char *queueinfo            = NULL;
  json_int_t nrunning        = 0;
  json_int_t nslots          = 0;
  json_int_t last_waited_for = 0;

  a2data = yapi_data(ctx);

  /* get the queueinfo from the knegd service */
  ret = yclcli_kneg_queueinfo(&a2data->kneg, &queueinfo);
  if (ret != YCL_OK) {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        yclcli_strerror(&a2data->kneg));
  }

  /* extract the values from the queueinfo response */
  if (queueinfo) {
    sscanf(queueinfo, "%" JSON_INTEGER_FORMAT " %" JSON_INTEGER_FORMAT
        " %" JSON_INTEGER_FORMAT, &nrunning, &nslots, &last_waited_for);
  }

  /* build the response JSON object */
  data = json_object();
  json_object_set_new(data, "nrunning", json_integer(nrunning));
  json_object_set_new(data, "nslots", json_integer(nslots));
  json_object_set_new(data, "last-waited-for",
      json_integer(last_waited_for));
  top = json_object();
  json_object_set_new(top, "success", json_true());
  json_object_set_new(top, "data", data);

  /* write the response */
  yapi_header(ctx, YAPI_STATUS_OK, YAPI_CTYPE_JSON);
  json_dumpf(top, ctx->output, JSON_ENSURE_ASCII|JSON_COMPACT);
  json_decref(top);
  return 0;
}

/*
 * GET /a1/work-types
 *
 * Response object on success:
 * {
 *   "success": true,
 *   "data": {
 *     "entries": [
 *       "entry0",
 *       "entry1",
 *       ...
 *       "entryN"
 *     ]
 *   }
 * }
 */
static int get_work_types(struct yapi_ctx *ctx) {
  struct a2_ctx *a2data = yapi_data(ctx);
  char *manifest        = NULL;
  int ret;
  json_t *entries;
  json_t *data;
  json_t *top;
  char *entry;


  /* get the kneg manifest from the knegd service */
  ret = yclcli_kneg_manifest(&a2data->kneg, &manifest);
  if (ret != YCL_OK) {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        yclcli_strerror(&a2data->kneg));
  } else if (!manifest) {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        "no kneg manifest");
  }

  /* build the response JSON object */
  entries = json_array();
  while ((entry = strsep(&manifest, "\n")) != NULL) {
    json_array_append_new(entries, json_string(entry));
  }
  data = json_object();
  json_object_set_new(data, "entries", entries);
  top = json_object();
  json_object_set_new(top, "success", json_true());
  json_object_set_new(top, "data", data);

  /* write the response */
  yapi_header(ctx, YAPI_STATUS_OK, YAPI_CTYPE_JSON);
  json_dumpf(top, ctx->output, JSON_ENSURE_ASCII|JSON_COMPACT);
  json_decref(top);
  return 0;
}

/*
 * GET /a1/reports
 *
 * URL Parameters:
 *   - nelems (opt): Number of elements to fetch. Range: 0 <= nelems <= 100
 *                   default: 25
 *   - before (opt): Fetch indices before a given row.
 *
 * Response object on success:
 * {
 *   "success": true,
 *   "data": {
 *     "now_ts": 666,
 *     "entries": [
 *       {
 *         "id": "70a7a85d0004f8b9c2be",
 *         "name": "bubbelibubb",
 *         "ts": 1530356368,
 *         "row": 0,
 *         "status": "str"
 *       },
 *       ...
 *     ]
 *   }
 * }
 */
static int get_reports(struct yapi_ctx *ctx) {
  struct a2_ctx *a2data = yapi_data(ctx);
  char *key             = NULL;
  char *val             = NULL;
  size_t nelems         = 25;
  size_t before         = 0;
  size_t last           = 0;
  char *statuses;
  char *status;
  int indexfd;
  int ret;
  FILE *fp;
  struct sindex_ctx si;
  struct sindex_entry *elems;
  ssize_t ss;
  ssize_t i;
  buf_t buf;
  json_t *obj;
  json_t *entries;
  json_t *data;
  json_t *top;

  /* parse URL query parameters */
  while (urlquery_next_pair(&ctx->req.query_string, &key, &val)) {
    if (strcmp(key, "nelems") == 0) {
      sscanf(key, "%zu", &nelems);
      nelems = MIN(nelems, 100);
    } else if (strcmp(key, "before") == 0) {
      sscanf(key, "%zu", &before);
    }
  }

  /* allocate memory for store index entries */
  elems = calloc(nelems, sizeof(struct sindex_entry));
  if (!elems) {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        "failed to allocate memory for index entries");
  }

  /* get the file handle for the store index */
  ret = yclcli_store_index(&a2data->store, before, nelems, &indexfd);
  if (ret != YCL_OK) {
    free(elems);
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        yclcli_strerror(&a2data->store));
  } else if ((fp = fdopen(indexfd, "r")) == NULL) {
    close(indexfd);
    free(elems);
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        "failed to open index for reading");
  }

  /* get the entries from the index */
  sindex_init(&si, fp);
  ss = sindex_get(&si, elems, nelems, before, &last);
  fclose(fp);
  if (ss < 0) {
    free(elems);
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        "failed to get index entries");
  } else if (ss > 0) {
    /* initialize buffer for status request to knegd */
    if (buf_init(&buf, 1024) == NULL) {
      free(elems);
      return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
          "failed to allocate status request buffer");
    }

    /* build knegd status request */
    for (i = 0; i < ss; i++) {
      buf_adata(&buf, elems[i].id, SINDEX_IDSZ);
      buf_achar(&buf, '\0');
    }

    /* request the statuses */
    ret = yclcli_kneg_status(&a2data->kneg, buf.data, buf.len, &statuses);
    buf_cleanup(&buf);
    if (ret != YCL_OK) {
      free(elems);
      return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
          yclcli_strerror(&a2data->kneg));
    }
  }

  /* build the entries array for the response */
  entries = json_array();
  for (i = 0; i < ss; i++) {
    status = strsep(&statuses, "\r\n\t ");
    if (!status) {
      status = "n/a";
    }

    obj = json_object();
    json_object_set_new(obj, "id", json_stringn(elems[i].id, SINDEX_IDSZ));
    json_object_set_new(obj, "name", json_string(elems[i].name));
    json_object_set_new(obj, "ts", json_integer(elems[i].indexed));
    json_object_set_new(obj, "row", json_integer(last++));
    json_object_set_new(obj, "status", json_string(status));
    json_array_append_new(entries, obj);
  }

  /* no need for the sindex elems any more - free them */
  free(elems);

  /* build top-level response object */
  data = json_object();
  json_object_set_new(data, "entries", entries);
  json_object_set_new(data, "now_ts", json_integer(time(NULL)));
  top = json_object();
  json_object_set_new(top, "success", json_true());
  json_object_set_new(top, "data", data);

  /* write the response */
  yapi_header(ctx, YAPI_STATUS_OK, YAPI_CTYPE_JSON);
  json_dumpf(top, ctx->output, JSON_ENSURE_ASCII|JSON_COMPACT);
  json_decref(top);
  return 0;
}

/*
 * GET /a1/report-section
 *
 * URL Parameters:
 *   - id:    ID of the given report.
 *   - name: Name of section
 *
 * If id or entry is missing or invalid, a 400 response is returned.
 *
 * Response on success: The report section.
 */
static int get_report_section(struct yapi_ctx *ctx) {
  struct a2_ctx *a2data   = yapi_data(ctx);
  int client_accepts_gzip = 0;
  int content_is_gzip     = 0;
  const char *id          = NULL;
  const char *name        = NULL;
  int sectionfd           = -1;
  int ctype               = YAPI_CTYPE_BINARY;
  FILE *fp;
  char buf[4096];
  ssize_t nread;
  char *key;
  char *val;
  char *cptr;
  int ret;

  /* parse URL query parameters */
  while (urlquery_next_pair(&ctx->req.query_string, &key, &val)) {
    if (strcmp(key, "id") == 0) {
      id = val;
    } else if (strcmp(key, "name") == 0) {
      name = val;
    }
  }

  /* validate query parameters */
  if (!name || !*name || !is_valid_id(id)) {
    return yapi_error(ctx, YAPI_STATUS_BAD_REQUEST,
        "missing/invalid request parameters");
  }

  /* enter store */
  ret = yclcli_store_enter(&a2data->store, id, NULL);
  if (ret != YCL_OK) {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        "store enter failure");
  }

  /* open the file referenced by name */
  ret = yclcli_store_open(&a2data->store, name, O_RDONLY, &sectionfd);
  if (ret != YCL_OK) {
    return yapi_error(ctx, YAPI_STATUS_BAD_REQUEST,
        "failed to open the resource with the specified name");
  }

  /* check name suffix for .gz, to determine if the content is gzipped.
   * if the name ends with .gz, strip it for later suffix checking */
  cptr = strrchr(name, '.');
  if (cptr && strcmp(cptr, ".gz") == 0) {
    content_is_gzip = 1;
    *cptr = '\0';
  }

  /* determine the content type (default: binary/octet-stream) */
  cptr = strrchr(name, '.');
  if (!cptr) {
    if (strcmp(name, "MANIFEST") == 0) {
      ctype = YAPI_CTYPE_TEXT;
    }
  } else if (strcmp(cptr, ".json") == 0) {
    ctype = YAPI_CTYPE_JSON;
  } else if (strcmp(cptr, ".csv") == 0) {
    ctype = YAPI_CTYPE_CSV;
  } else if (strcmp(cptr, ".txt") == 0 ||
             strcmp(cptr, ".log") == 0) {
    ctype = YAPI_CTYPE_TEXT;
  }

  /* determine if the client supports gzipped transfers by looking at the
   * HTTP_ACCEPT_ENCODING/Accept-Encoding header value */
  if (ctx->req.accept_encoding &&
      strstr(ctx->req.accept_encoding, "gzip")) {
    /* XXX: doesn't take qvalues into account, e.g., q=0 */
    client_accepts_gzip = 1;
  }

  /* We decompress the file on-the-fly if the client does not accept
   * gzip-encoded transfers */
  if (content_is_gzip && !client_accepts_gzip) {
    fp = zfile_fdopen(sectionfd, "rb");
  } else {
    fp = fdopen(sectionfd, "rb");
  }

  if (fp == NULL) {
    close(sectionfd);
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        "failed to open a handle of the underlying file descriptor");
  }

  /* send the appropriate headers */
  if (content_is_gzip && client_accepts_gzip) {
    yapi_headers(ctx, YAPI_STATUS_OK, ctype, "Content-Encoding: gzip",
        NULL);
  } else {
    yapi_header(ctx, YAPI_STATUS_OK, ctype);
  }

  /* send the content */
  while ((nread = fread(buf, 1, sizeof(buf), fp)) > 0) {
    yapi_write(ctx, buf, nread);
  }

  fclose(fp);
  return 0;
}

/*
 * GET /a1/report-sections
 *
 * URL Parameters:
 *   - id: A (valid) report ID.
 *
 * Response object on success:
 * {
 *   "success": true,
 *   "data": {
 *     "id": "str",
 *     "name": "str",
 *     "subject": "str",
 *     "indexed": 666,
 *     "now_ts": 777,
 *     "status": "str",
 *     "entries": [
 *       {
 *         "fname": "fname",
 *         "dname": "dname"
 *       },
 *       ...
 *     ]
 *   }
 * }
 */
static int get_report_sections(struct yapi_ctx *ctx) {
  struct a2_ctx *a2data = yapi_data(ctx);
  char *id              = NULL;
  json_t *data          = NULL;
  char linebuf[256];
  char *key;
  char *val;
  int ret;
  FILE *fp;
  json_error_t json_err;
  json_t *entries;
  json_t *obj;
  json_t *top;

  /* parse URL query parameters */
  while (urlquery_next_pair(&ctx->req.query_string, &key, &val)) {
    if (strcmp(key, "id") == 0) {
      id = val;
    }
  }

  /* validate query parameters */
  if (!is_valid_id(id)) {
    return yapi_error(ctx, YAPI_STATUS_BAD_REQUEST,
        "missing/invalid request parameters");
  }

  /* enter store */
  ret = yclcli_store_enter(&a2data->store, id, NULL);
  if (ret != YCL_OK) {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        "store enter failure");
  }

  /* create an 'entries' array, and fill it with entries from the
   * MANIFEST file, if possible */
  entries = json_array();
  ret = yclcli_store_fopen(&a2data->store, "MANIFEST", "rb", &fp);
  if (ret == YCL_OK) {
    while (fgets(linebuf, sizeof(linebuf), fp)) {
      /* remove trailing newline */
      val = strrchr(linebuf, '\n');
      if (val) {
        *val = '\0';
      }

      /* set 'val' to the display name of the row */
      val = strchr(linebuf, ' ');
      if (!val) {
        continue;
      } else {
        *val++ = '\0';
      }

      obj = json_object();
      json_object_set_new(obj, "fname", json_string(linebuf));
      json_object_set_new(obj, "dname", json_string(val));
      json_array_append_new(entries, obj);
    }
    fclose(fp);
    fp = NULL;
  }

  /* if possible: build the 'data' object from the job.json file */
  ret = yclcli_store_fopen(&a2data->store, "job.json", "rb", &fp);
  if (ret == YCL_OK) {
    data = json_loadf(fp, 0, &json_err);
    fclose(fp);
  }

  /* If we were unable to build the 'data' object from the job.json file,
   * we initialize it to an empty object */
  if (!data) {
    data = json_object();
  }

  /* get the kneg status. NB: reuses the message buffer. If subsequent
   * ycl calls are made, 'val' will no longer be valid */
  ret = yclcli_kneg_status(&a2data->kneg, id, id ? strlen(id) : 0, &val);
  if (ret != YCL_OK) {
    val = "unknown";
  }

  /* build the JSON response object */
  json_object_set_new(data, "id", json_string(id));
  json_object_set_new(data, "now_ts", json_integer(time(NULL)));
  json_object_set_new(data, "status", json_string(val));
  json_object_set_new(data, "entries", entries);
  top = json_object();
  json_object_set_new(top, "success", json_true());
  json_object_set_new(top, "data", data);

  /* write the response */
  yapi_header(ctx, YAPI_STATUS_OK, YAPI_CTYPE_JSON);
  json_dumpf(top, ctx->output, JSON_ENSURE_ASCII|JSON_COMPACT);
  json_decref(top);
  return 0;
}

static int queue_work(struct yapi_ctx *ctx, const char *sub, size_t sublen,
    const char *id, const char *type, const char *name) {
  struct a2_ctx *a2data = yapi_data(ctx);
  int ret;
  FILE *fp;
  size_t len;

  /* write the subject entry to subject.txt */
  ret = yclcli_store_fopen(&a2data->store, "subject.txt", "wb", &fp);
  if (ret == YCL_OK) {
    len = fwrite(sub, 1, sublen, fp);
    fclose(fp);
    if (len < sublen) {
      return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
          "failed to save subject list");
    }
  } else {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        yclcli_strerror(&a2data->store));
  }

  /* queue the work! */
  ret = yclcli_kneg_queue(&a2data->kneg, id, type, name, 0, NULL);
  if (ret != YCL_OK) {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        yclcli_strerror(&a2data->kneg));
  }

  return 0;
}

/*
 * POST /a1/scan
 * Content-Type: text/plain
 *
 * URL Parameters:
 *   - name (opt): A name, will get truncated if too long
 *   - type (opt): Type of job to perform, defaults to DFL_KNEG_TYPE
 *
 * Returns 200 OK on success, with the store ID as the response body
 */
static int post_scan_text(struct yapi_ctx *ctx) {
  struct a2_ctx *a2data = yapi_data(ctx);
  buf_t reqbody         = {0};
  char namebuf[40]      = {0};
  const char *type      = DFL_KNEG_TYPE;
  const char *tmp;
  char *key;
  char *val;
  char idbuf[64];
  size_t len;
  int ret;

  /* parse URL query parameters */
  while (urlquery_next_pair(&ctx->req.query_string, &key, &val)) {
    if (strcmp(key, "name") == 0) {
      len = strlen(val);
      if (len < sizeof(namebuf)) {
        snprintf(namebuf, sizeof(namebuf), "%.*s", (int)len, val);
      } else {
        snprintf(namebuf, sizeof(namebuf), "%.*s...", (int)len - 4, val);
      }
    } else if (strcmp(key, "type") == 0) {
      type = val;
    }
  }

  /* initialize request body buffer */
  if (!buf_init(&reqbody, 8192)) {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        "failed to allocate receive buffer");
  }

  /* read the request body and make sure we have some minimal amount of
   *  data at this point */
  yapi_read(ctx, &reqbody);
  if (reqbody.len < 3) {
    buf_cleanup(&reqbody);
    return yapi_error(ctx, YAPI_STATUS_BAD_REQUEST,
        "missing request body");
  }

  /* enter the store, get an ID */
  ret = yclcli_store_enter(&a2data->store, NULL, &tmp);
  if (ret != YCL_OK) {
    buf_cleanup(&reqbody);
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        yclcli_strerror(&a2data->store));
  } else {
    strncpy(idbuf, tmp, sizeof(idbuf));
    idbuf[sizeof(idbuf)-1] = '\0';
  }

  /* If a name was not supplied as an URL parameter, set it to the ID
   * of the store now */
  if (namebuf[0] == '\0') {
    snprintf(namebuf, sizeof(namebuf), "%s", idbuf);
  }

  ret = queue_work(ctx, reqbody.data, reqbody.len,
    idbuf, type, namebuf);
  if (ret != 0) {
    return ret;
  }

  yapi_header(ctx, YAPI_STATUS_OK, YAPI_CTYPE_TEXT);
  yapi_write(ctx, idbuf, strlen(idbuf));
  return 0;
}

/*
 * POST /a1/scan
 * Content-Type: application/json
 *
 * Request object:
 * {
 *   "..."
 * }
 *
 * Response object on success:
 * {
 *   "success": true,
 *   "data": {
 *
 *   }
 * }
 */
static int post_scan_json(struct yapi_ctx *ctx) {
  struct a2_ctx *a2data = yapi_data(ctx);
  const char *parserr   = "Invalid scan request";
  buf_t reqbody         = {0};
  buf_t subjectbuf      = {0};
  size_t width          = 0;
  char namebuf[40];
  char idbuf[64];
  json_t *req;
  json_t *jstr;
  json_error_t jsonerr;
  const char *subject;
  const char *type;
  const char *tmp;
  size_t len;
  size_t i;
  int ch;
  int ret;
  FILE *fp;
  json_t *top;
  json_t *data;

  /* initialize the request body receive buffer */
  if (!buf_init(&reqbody, 8192)) {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        "failed to allocate receive buffer");
  }

  /* initialize the subject buffer - clean it up whenever reqbody is
   * cleaned up */
  if (!buf_init(&subjectbuf, 8192)) {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        "failed to allocate subject buffer");
  }

  /* read and parse the JSON request */
  yapi_read(ctx, &reqbody);
  req = json_loads(reqbody.data, 0, &jsonerr);
  buf_cleanup(&reqbody);
  if (!req) {
    goto invalid_req;
  }

  /* retrieve the subject value, or bail if missing */
  jstr = json_object_get(req, "subject");
  if (!jstr) {
    goto invalid_req;
  }

  subject = json_string_value(jstr);
  len = json_string_length(jstr);
  if (len == 0) {
    goto invalid_req;
  }

  /* make lowercase */
  for (i = 0; i < len; i += width) {
    ch = u8_to_cp(subject + i, len - i, &width);
    ch = u8_tolower(ch);
    ret = buf_reserve(&subjectbuf, 8);
    if (ret < 0) {
      parserr = "Failed to reserve ucp-buffer";
      goto invalid_req; /* Not actually an invalid request */
    } else {
      u8_from_cp(subjectbuf.data + subjectbuf.len, 8, ch, &width);
      subjectbuf.len += width;
    }
  }

  subject = NULL; /* don't use this anymore - use subjectbuf */

  /* get the kneg type, or default to DFL_KNEG_TYPE */
  jstr = json_object_get(req, "type");
  if (!jstr || json_string_length(jstr) == 0) {
    type = DFL_KNEG_TYPE;
  } else {
    type = json_string_value(jstr);
  }

  /* create a suitable name */
  if (subjectbuf.len < sizeof(namebuf)) {
    snprintf(namebuf, sizeof(namebuf), "%.*s", (int)subjectbuf.len,
        subjectbuf.data);
  } else {
    snprintf(namebuf, sizeof(namebuf), "%.*s...", (int)sizeof(namebuf) -  4,
        subjectbuf.data);
  }

  /* update request fields and free subjectbuf */
  json_object_set_new(req, "name", json_string(namebuf));
  json_object_set_new(req, "subject", json_stringn(subjectbuf.data,
      subjectbuf.len));
  json_object_set_new(req, "started", json_integer(time(NULL)));
  buf_cleanup(&subjectbuf);

  /* enter store */
  ret = yclcli_store_enter(&a2data->store, NULL, &tmp);
  if (ret != YCL_OK) {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        yclcli_strerror(&a2data->store));
  } else {
    strncpy(idbuf, tmp, sizeof(idbuf));
    idbuf[sizeof(idbuf)-1] = '\0';
  }

  /* write the request to job.json */
  ret = yclcli_store_fopen(&a2data->store, "job.json", "wb", &fp);
  if (ret == YCL_OK) {
    ret = json_dumpf(req, fp, 0);
    fclose(fp);
    if (ret < 0) {
      return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
          "failed to store request to disk");
    }
  }

  /* queue the work! */
  jstr = json_object_get(req, "subject");
  ret = queue_work(ctx,json_string_value(jstr), json_string_length(jstr),
    idbuf, type, namebuf);
  json_decref(req);
  req = NULL;
  if (ret != 0) {
    return ret;
  }

  /* build and write the response */
  data = json_object();
  json_object_set_new(data, "id", json_string(idbuf));
  top = json_object();
  json_object_set_new(top, "success", json_true());
  json_object_set_new(top, "data", data);
  yapi_header(ctx, YAPI_STATUS_OK, YAPI_CTYPE_JSON);
  json_dumpf(top, ctx->output, JSON_ENSURE_ASCII|JSON_COMPACT);
  json_decref(top);

  return 0;

invalid_req:
  buf_cleanup(&subjectbuf);
  /* TODO: Error feedback from jsonerr, if relevant? */
  return yapi_error(ctx, YAPI_STATUS_BAD_REQUEST, parserr);
}

static int post_scan(struct yapi_ctx *ctx) {
  enum yapi_ctype ctype = YAPI_CTYPE_NONE;

  /* Determine and validate Content-Type */
  if (!ctx->req.content_type || *ctx->req.content_type == '\0') {
    return yapi_error(ctx, YAPI_STATUS_BAD_REQUEST,
        "missing content-type");
  } else if (strstr(ctx->req.content_type, "application/json")) {
    ctype = YAPI_CTYPE_JSON;
  } else if (strstr(ctx->req.content_type, "text/plain")) {
    ctype = YAPI_CTYPE_TEXT;
  } else {
  }

  switch (ctype) {
    case YAPI_CTYPE_JSON:
      return post_scan_json(ctx);
    case YAPI_CTYPE_TEXT:
      return post_scan_text(ctx);
    default:
      return yapi_error(ctx, YAPI_STATUS_BAD_REQUEST,
          "invalid content-type");
  }
}

SC2MOD_API int sc2_setup(struct sc2mod_ctx *mod) {
  struct a2_ctx *a2data;
  int ret;

  a2data = &ctx_;
  ret = ycl_msg_init(&a2data->msgbuf);
  if (ret != YCL_OK) {
    return sc2mod_error(mod, "failed to initialize ycl message buffer");
  }

  /* TODO: Get socket paths from sc2mod_ctx instead, but without sc2mod_ctx
   *       knowing about what yclclis are used &c */

  /* Initialize kneg YCL client */
  yclcli_init(&a2data->kneg, &a2data->msgbuf);
  ret = yclcli_connect(&a2data->kneg, "knegd/knegd.sock");
  if (ret != YCL_OK) {
    ycl_msg_cleanup(&a2data->msgbuf);
    return sc2mod_error(mod, yclcli_strerror(&a2data->kneg));
  }

  /* Initialize store YCL client */
  yclcli_init(&a2data->store, &a2data->msgbuf);
  ret = yclcli_connect(&a2data->store, "stored/stored.sock");
  if (ret != YCL_OK) {
    ycl_msg_cleanup(&a2data->msgbuf);
    yclcli_close(&a2data->kneg);
    return sc2mod_error(mod, yclcli_strerror(&a2data->store));
  }

  sc2mod_set_data(mod, a2data);
  return 0;
}

static void cleanup(struct a2_ctx *a2data) {
  yclcli_close(&a2data->store);
  yclcli_close(&a2data->kneg);
  ycl_msg_cleanup(&a2data->msgbuf);
}

SC2MOD_API int sc2_handler(struct sc2mod_ctx *mod) {
  int ret;
  struct yapi_ctx ctx;
  struct yapi_route routes[] = {
    {YAPI_METHOD_GET,  "fail",            get_fail},
    {YAPI_METHOD_GET,  "queueinfo",       get_queueinfo},
    {YAPI_METHOD_GET,  "work-types",      get_work_types},
    {YAPI_METHOD_GET,  "reports",         get_reports},
    {YAPI_METHOD_GET,  "report-section",  get_report_section},
    {YAPI_METHOD_GET,  "report-sections", get_report_sections},
    {YAPI_METHOD_POST, "scan",            post_scan},
  };

  yapi_init(&ctx);
  yapi_set_data(&ctx, sc2mod_data(mod));
  ret = yapi_serve(&ctx, "/a1/", routes, ARRAY_SIZE(routes));
  cleanup(sc2mod_data(mod));
  return ret;
}

