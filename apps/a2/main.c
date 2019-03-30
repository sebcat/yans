#include <string.h>
#include <unistd.h>

#include <jansson.h>

#include <lib/ycl/yclcli_kneg.h>
#include <lib/ycl/yclcli_store.h>

#include <lib/util/sindex.h>
#include <lib/util/macros.h>
#include <lib/util/sc2mod.h>
#include <lib/net/urlquery.h>
#include <apps/a2/yapi.h>

struct a2_ctx {
  struct ycl_msg msgbuf;
  struct yclcli_ctx kneg;
  struct yclcli_ctx store;
};

struct a2_ctx ctx_; /* should only be accessed by sc2_setup if we want to
                     * allocate it dynamically in the future */

static int get_fail(struct yapi_ctx *ctx) {
  return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR, "got fail");
}

/*
 * GET /trololo/queueinfo
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
 * GET /trololo/work-types
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
  struct a2_ctx *a2data;
  int ret;
  char *manifest = NULL;
  json_t *entries;
  json_t *data;
  json_t *top;
  char *entry;

  a2data = yapi_data(ctx);

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
  }

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
  ret = yclcli_connect(&a2data->kneg, KNEGCLI_DFLPATH);
  if (ret != YCL_OK) {
    ycl_msg_cleanup(&a2data->msgbuf);
    return sc2mod_error(mod, yclcli_strerror(&a2data->kneg));
  }

  /* Initialize store YCL client */
  yclcli_init(&a2data->store, &a2data->msgbuf);
  ret = yclcli_connect(&a2data->store, STORECLI_DFLPATH);
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
    {YAPI_METHOD_GET, "fail",       get_fail},
    {YAPI_METHOD_GET, "queueinfo",  get_queueinfo},
    {YAPI_METHOD_GET, "work-types", get_work_types},
    {YAPI_METHOD_GET, "reports",    get_reports},
  };

  yapi_init(&ctx);
  yapi_set_data(&ctx, sc2mod_data(mod));
  ret = yapi_serve(&ctx, "/trololo/", routes, ARRAY_SIZE(routes));
  cleanup(sc2mod_data(mod));
  return ret;
}

