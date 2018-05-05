#include <lib/lua/sc2.h>

/*
 * TOS: table with the following values:
 *   - path: required, unix socket path
 *   - maxreqs: optional, maximum number of concurrent requests. Defaults to
 *              DEFAULT_MAXREQS
 *   - lifetime: optional, maximum number of seconds to serve a request.
 *               Defaults to DEFAULT_LIFETIME
 *   - rlimit_vmem: optional, child RLIMIT_VMEM/RLIMIT_AS, in kbytes. Defaults
 *                  to DEFAULT_RLIMIT_VMEM
 *   - rlimit_cpu: optional, child RLIMIT_CPU, in seconds. Defaults to
 *                 DEFAULT_RLIMIT_CPU
 *   - servefunc: required, fn(o). Called in child for each accepted client.
 *   - donefunc: required, fn(o). Called in parent on successful/failed
 *               request
 */
static int l_sc2_serve(lua_State *L) {
  return 0;
}

static const struct luaL_Reg sc2_f[] = {
  {"serve", l_sc2_serve},
  {NULL, NULL},
};

int luaopen_sc2(lua_State *L) {
  luaL_newlib(L, sc2_f);
  return 1;
}
