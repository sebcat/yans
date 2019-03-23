#include <unistd.h>

#include <lib/util/macros.h>
#include <apps/a2/yapi.h>

#define SC2APIFUNC __attribute__((visibility("default")))

static int get_a(struct yapi_ctx *ctx) {
  return 10;
}

static int post_b(struct yapi_ctx *ctx) {
  return 11;
}

static int get_b(struct yapi_ctx *ctx) {
  return 12;
}

SC2APIFUNC void *sc2_setup(void) {
  return "icanhasdata";
}

SC2APIFUNC int sc2_handler(void *data) {
  struct yapi_ctx rutt;
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

  yapi_init(&rutt);
  yapi_set_input(&rutt, STDIN_FILENO);
  yapi_set_output(&rutt, STDOUT_FILENO);
  return yapi_serve(&rutt, "/trololo/", routes, ARRAY_SIZE(routes));
}

