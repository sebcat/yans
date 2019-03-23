#include <lib/util/macros.h>
#include <apps/a2/yapi.h>

#define SC2APIFUNC __attribute__((visibility("default")))

static int get_a(struct yapi_ctx *ctx) {
  yapi_header(ctx, YAPI_STATUS_OK, YAPI_CTYPE_TEXT);
  yapi_write(ctx, "get a\n", sizeof("get a\n")-1);
  return 10;
}

static int post_b(struct yapi_ctx *ctx) {
  yapi_header(ctx, YAPI_STATUS_OK, YAPI_CTYPE_CSV);
  yapi_write(ctx, "foo,bar\n", sizeof("foo,bar\n")-1);
  return 11;
}

static int get_b(struct yapi_ctx *ctx) {
  yapi_header(ctx, YAPI_STATUS_OK, YAPI_CTYPE_TEXT);
  yapi_write(ctx, "get b\n", sizeof("get b\n")-1);
  return 12;
}

SC2APIFUNC void *sc2_setup(void) {
  return "icanhasdata";
}

SC2APIFUNC int sc2_handler(void *data) {
  struct yapi_ctx ctx;
  struct yapi_route routes[] = {
    {
      .method = YAPI_METHOD_GET,
      .path   = "a",
      .func   = get_a,
    },
    {
      .method = YAPI_METHOD_POST,
      .path   = "b",
      .func   = post_b,
    },
    {
      .method = YAPI_METHOD_GET,
      .path   = "b",
      .func   = get_b,
    }
  };

  yapi_init(&ctx);
  yapi_set_data(&ctx, data);
  return yapi_serve(&ctx, "/trololo/", routes, ARRAY_SIZE(routes));
}

