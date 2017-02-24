#include <lib/lua/pcapd.h>

#define MTNAME_PCAPD "pcapd.pcapd"

#define checkpcapd(L, i) \
    ((pcapd_t*)luaL_checkudata(L, (i), MTNAME_PCAPD))

/* create a new pcapd_t handle and connect to pcapd */
static int l_pcapd_connect(lua_State *L) {

}

/* open a new dump */
static int l_pcapd_open(lua_State *L) {

}

static const struct luaL_Reg pcapd_type[] = {
  {"open", l_pcapd_open},
  {NULL, NULL}
};

static const struct luaL_Reg pcapd_lib[] = {
  {"connect", l_pcapd_connect},
  {NULL, NULL}
};

int luaopen_pcapd(lua_State *L) {
  luaL_newmetatable(L, MTNAME_PCAPD);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, pcapd_type, 0);
  luaL_newlib(L, pcapd_lib);
  return 1;
}

