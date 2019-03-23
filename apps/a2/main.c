#include <lib/util/macros.h>
#include <apps/a2/yapi.h>

static int get_fail(struct yapi_ctx *ctx) {
  return yapi_error(ctx, YAPI_STATUS_INTERNAL_SERVER_ERROR, "got fail");
}

APIFUNC void *sc2_setup(void) {
  return "icanhasdata";
}

APIFUNC int sc2_handler(void *data) {
  struct yapi_ctx ctx;
  struct yapi_route routes[] = {
    {YAPI_METHOD_GET, "fail", get_fail}
  };

  yapi_init(&ctx);
  yapi_set_data(&ctx, data);
  return yapi_serve(&ctx, "/trololo/", routes, ARRAY_SIZE(routes));
}

