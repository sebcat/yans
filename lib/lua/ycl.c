#include <lib/ycl/ycl.h>
#include <lib/lua/ycl.h>

#define MTNAME_YCLMSG "yans.YCLMsg"
#define MTNAME_YCLCTX "yans.YCLCtx"

#define checkmsg(L, i) \
  ((struct ycl_msg *)luaL_checkudata(L, (i), MTNAME_YCLMSG))

#define checkctx(L, i) \
  ((struct ycl_ctx *)luaL_checkudata(L, (i), MTNAME_YCLCTX))

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

static int l_yclctx(lua_State *L) {
  struct ycl_ctx *ctx;
  int fd;

  fd = (int)luaL_checkinteger(L, 1);
  if (fd < 0) {
    return luaL_error(L, "invalid file descriptor");
  }

  ctx = lua_newuserdata(L, sizeof(struct ycl_ctx));
  luaL_setmetatable(L, MTNAME_YCLCTX);
  ycl_init(ctx, fd);

  /* for the eds use case, the file descriptor is managed externally and
   * closed implicitly by eds. If ycl closes its fd on gc, that may occur
   * after a new client connects, receiving the same fd number. So if we
   * create a ycl_ctx with an externally opened file descriptor, the fd should
   * be closed externally too. Again, for our current use case. */
  ycl_set_externalfd(ctx);
  return 1;
}

static int l_yclconnect(lua_State *L) {
  struct ycl_ctx *ctx;
  const char *dst;

  dst = luaL_checkstring(L, 1);
  ctx = lua_newuserdata(L, sizeof(struct ycl_ctx));
  luaL_setmetatable(L, MTNAME_YCLCTX);
  if (ycl_connect(ctx, dst) != YCL_OK) {
    return luaL_error(L, "%s", ycl_strerror(ctx));
  }

  return 1;
}

static int l_ctxgc(lua_State *L) {
  struct ycl_ctx *ctx;

  ctx = checkctx(L, 1);
  ycl_close(ctx);
  return 0;
}

static int l_ctxsetnonblock(lua_State *L) {
  struct ycl_ctx *ctx;
  int status;

  ctx = checkctx(L, 1);
  status = luaL_checkinteger(L, 2);
  if (ycl_setnonblock(ctx, status) != YCL_OK) {
    return luaL_error(L, "%s", ycl_strerror(ctx));
  }
  return 0;
}

static int l_ctxsendmsg(lua_State *L) {
  struct ycl_ctx *ctx;
  struct ycl_msg *msg;
  int ret;

  ctx = checkctx(L, 1);
  msg = checkmsg(L, 2);
  ret = ycl_sendmsg(ctx, msg);
  if (ret == YCL_OK || ret == YCL_AGAIN) {
    lua_pushinteger(L, (lua_Integer)ret);
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, YCL_ERR);
    lua_pushstring(L, ycl_strerror(ctx));
  }

  return 2;
}

static int l_ctxrecvmsg(lua_State *L) {
  struct ycl_ctx *ctx;
  struct ycl_msg *msg;
  int ret;

  ctx = checkctx(L, 1);
  msg = checkmsg(L, 2);
  ret = ycl_recvmsg(ctx, msg);
  if (ret == YCL_OK || ret == YCL_AGAIN) {
    lua_pushinteger(L, (lua_Integer)ret);
    lua_pushnil(L);
  } else {
    lua_pushinteger(L, YCL_ERR);
    lua_pushstring(L, ycl_strerror(ctx));
  }

  return 2;
}

static const struct luaL_Reg yclmsg_m[] = {
  {"__gc", l_yclmsggc},
  {"size", l_yclmsgsize},
  {"data", l_yclmsgdata},
  {"create_status_resp", l_createstatusresp},
  {"parse_status_resp", l_parsestatusresp},
  {NULL, NULL},
};

static const struct luaL_Reg yclctx_m[] = {
  {"__gc", l_ctxgc},
  {"setnonblock", l_ctxsetnonblock},
  {"sendmsg", l_ctxsendmsg},
  {"recvmsg", l_ctxrecvmsg},
  {NULL, NULL},
};

static const struct luaL_Reg ycl_f[] = {
  {"ctx", l_yclctx},
  {"connect", l_yclconnect},
  {"msg", l_yclmsg},
  {NULL, NULL},
};

int luaopen_ycl(lua_State *L) {
  size_t i;
  struct {
    const char *name;
    const struct luaL_Reg *reg;
  } mts[] = {
    {MTNAME_YCLCTX, yclctx_m},
    {MTNAME_YCLMSG, yclmsg_m},
    {NULL, NULL},
  };
  struct {
    const char *name;
    lua_Integer val;
  } consts[] = {
    {"AGAIN", YCL_AGAIN},
    {"ERROR", YCL_ERR},
    {"OK", YCL_OK},
    {NULL, 0},
  };

  for (i = 0; mts[i].name != NULL; i++) {
    luaL_newmetatable(L, mts[i].name);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, mts[i].reg, 0);
  }

  luaL_newlib(L, ycl_f);

  for (i = 0; consts[i].name != NULL; i++) {
    lua_pushinteger(L, consts[i].val);
    lua_setfield(L, -2, consts[i].name);
  }

  return 1;
}
