#include <lib/ycl/ycl.h>
#include <lib/lua/ycl.h>

#define MTNAME_YCLMSG "yans.YCLMsg"

#define checkmsg(L, i) \
  ((struct ycl_msg *)luaL_checkudata(L, (i), MTNAME_YCLMSG))

#define SERIALIZATION_ERROR "YCL serialization error"
#define PARSE_ERROR "YCL parse error"

static int l_yclmsggc(lua_State *L) {
  struct ycl_msg *msg;

  msg = checkmsg(L, 1);
  ycl_msg_cleanup(msg);
  return 0;
}

static int l_yclmsg(lua_State *L) {
  struct ycl_msg *msg;

  msg = lua_newuserdata(L, sizeof(struct ycl_msg));
  ycl_msg_init(msg);
  luaL_setmetatable(L, MTNAME_YCLMSG);
  return 1;
}

static int l_yclmsgsize(lua_State *L) {
  struct ycl_msg *msg;

  msg = checkmsg(L, 1);
  lua_pushinteger(L, (lua_Integer)msg->buf.len);
  return 1;
}

static int l_yclmsgdata(lua_State *L) {
  struct ycl_msg *msg;

  msg = checkmsg(L, 1);
  lua_pushlstring(L, msg->buf.data, msg->buf.len);
  return 1;
}

static int l_createstatusresp(lua_State *L) {
  struct ycl_msg *msg;
  struct ycl_status_resp r = {0};

  msg = checkmsg(L, 1);
  luaL_checktype(L, 2, LUA_TTABLE);

  if (lua_getfield(L, -1, "okmsg") == LUA_TSTRING) {
    r.okmsg = lua_tostring(L, -1);
  }
  lua_pop(L, 1);

  if (lua_getfield(L, -1, "errmsg") == LUA_TSTRING) {
    r.errmsg = lua_tostring(L, -1);
  }
  lua_pop(L, 1);

  if (ycl_msg_create_status_resp(msg, &r) != YCL_OK) {
    return luaL_error(L, SERIALIZATION_ERROR);
  }

  return 0;
}

static int l_parsestatusresp(lua_State *L) {
  struct ycl_msg *msg;
  struct ycl_status_resp r = {0};

  msg = checkmsg(L, 1);
  if (ycl_msg_parse_status_resp(msg, &r) != YCL_OK) {
    return luaL_error(L, PARSE_ERROR);
  }

  lua_newtable(L);
  if (r.okmsg != NULL) {
    lua_pushstring(L, r.okmsg);
    lua_setfield(L, -2, "okmsg");
  }

  if (r.errmsg != NULL) {
    lua_pushstring(L, r.errmsg);
    lua_setfield(L, -2, "errmsg");
  }

  ycl_msg_reset(msg);
  return 1;
}

static struct luaL_Reg yclmsg_m[] = {
  {"__gc", l_yclmsggc},
  {"size", l_yclmsgsize},
  {"data", l_yclmsgdata},
  {"create_status_resp", l_createstatusresp},
  {"parse_status_resp", l_parsestatusresp},
  {NULL, NULL},
};

static struct luaL_Reg ycl_f[] = {
  {"msg", l_yclmsg},
  {NULL, NULL},
};

int luaopen_ycl(lua_State *L) {
  luaL_newmetatable(L, MTNAME_YCLMSG);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, yclmsg_m, 0);

  luaL_newlib(L, ycl_f);
  return 1;
}
