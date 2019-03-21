#include <unistd.h>

#include <lib/util/macros.h>
#include <apps/a2/rutt.h>

#define SC2APIFUNC __attribute__((visibility("default")))

SC2APIFUNC void *sc2_setup(void) {
  return "icanhasdata";
}

SC2APIFUNC int sc2_handler(void *data) {
  struct rutt_ctx rutt;
  struct rutt_route routes[] = {

  };

  rutt_init(&rutt);
  rutt_set_input(&rutt, STDIN_FILENO);
  rutt_set_output(&rutt, STDOUT_FILENO);
  return rutt_serve(&rutt, "/a2/", routes, ARRAY_SIZE(routes));
}

