#include <lib/util/ylog.h>
#include <lib/lua/ylog.h>

static int l_init(lua_State *L) {
  const char *ident = luaL_checkstring(L, 1);
  int logger = (int)luaL_checkinteger(L, 2);
  ylog_init(ident, logger);
  return 0;
}

static int format_str(lua_State *L) {
  lua_getglobal(L, "string");
  if (lua_getfield(L, -1, "format") != LUA_TFUNCTION) {
    return luaL_error(L, "unable to obtain format function");
  }

  lua_insert(L, 1);
  lua_pop(L, 1); /* pop 'string' table */
  lua_call(L, lua_gettop(L) - 1, 1);
  return 1;
}

static int l_info(lua_State *L) {
  format_str(L);
  ylog_info("%s", lua_tostring(L, -1));
  return 0;
}

static int l_error(lua_State *L) {
  format_str(L);
  ylog_error("%s", lua_tostring(L, -1));
  return 0;
}

static const struct luaL_Reg ylog_f[] = {
  {"init", l_init},
  {"info", l_info},
  {"error", l_error},
  {NULL, NULL},
};

int luaopen_ylog(lua_State *L) {
  luaL_newlib(L, ylog_f);
  lua_pushinteger(L, YLOG_SYSLOG);
  lua_setfield(L, -2, "SYSLOG");
  lua_pushinteger(L, YLOG_STDERR);
  lua_setfield(L, -2, "STDERR");
  return 1;
}
