#include <string.h>

#include <jansson.h>

#include <lib/ycl/yclcli_kneg.h>

#include <lib/util/macros.h>
#include <lib/util/sc2mod.h>
#include <lib/net/urlquery.h>
#include <apps/a2/yapi.h>

struct a2_ctx {
  struct ycl_msg msgbuf;
  struct yclcli_ctx kneg;
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
  char *queueinfo = NULL;
  json_int_t nrunning        = 0;
  json_int_t nslots          = 0;
  json_int_t last_waited_for = 0;

  a2data = yapi_data(ctx);
  ret = yclcli_kneg_queueinfo(&a2data->kneg, &queueinfo);
  if (ret != YCL_OK) {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        yclcli_strerror(&a2data->kneg));
  }

  if (queueinfo) {
    sscanf(queueinfo, "%" JSON_INTEGER_FORMAT " %" JSON_INTEGER_FORMAT
        " %" JSON_INTEGER_FORMAT, &nrunning, &nslots, &last_waited_for);
  }

  data = json_object();
  json_object_set_new(data, "nrunning", json_integer(nrunning));
  json_object_set_new(data, "nslots", json_integer(nslots));
  json_object_set_new(data, "last-waited-for",
      json_integer(last_waited_for));
  top = json_object();
  json_object_set_new(top, "success", json_true());
  json_object_set_new(top, "data", data);

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
  ret = yclcli_kneg_manifest(&a2data->kneg, &manifest);
  if (ret != YCL_OK) {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        yclcli_strerror(&a2data->kneg));
  }

  if (!manifest) {
    return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR,
        "no kneg manifest");
  }

  entries = json_array();
  while ((entry = strsep(&manifest, "\n")) != NULL) {
    json_array_append_new(entries, json_string(entry));
  }

  data = json_object();
  json_object_set_new(data, "entries", entries);
  top = json_object();
  json_object_set_new(top, "success", json_true());
  json_object_set_new(top, "data", data);

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

  yclcli_init(&a2data->kneg, &a2data->msgbuf);
  /* TODO: Get socket path from sc2mod_ctx instead, but without sc2mod_ctx
   *       knowing about what yclclis are used &c */
  ret = yclcli_connect(&a2data->kneg, KNEGCLI_DFLPATH);
  if (ret != YCL_OK) {
    ycl_msg_cleanup(&a2data->msgbuf);
    return sc2mod_error(mod, yclcli_strerror(&a2data->kneg));
  }

  sc2mod_set_data(mod, a2data);
  return 0;
}

static void cleanup(struct a2_ctx *a2data) {
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
  };

  yapi_init(&ctx);
  yapi_set_data(&ctx, sc2mod_data(mod));
  ret = yapi_serve(&ctx, "/trololo/", routes, ARRAY_SIZE(routes));
  cleanup(sc2mod_data(mod));
  return ret;
}

