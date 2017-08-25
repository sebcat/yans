#include <string.h>

#include <lib/net/url.h>
#include <lib/lua/http.h>

#define MTNAME_URLBUILDER "yans.URLBuilder"

#define checkurlbuilder(L, i) \
    (*(url_ctx_t**)luaL_checkudata(L, (i), MTNAME_URLBUILDER))

static int l_urlbuilder(lua_State *L) {
  struct url_opts opts;
  struct url_ctx_t **ctx;
  int flags = 0;

  if (lua_type(L, 1) == LUA_TTABLE) {
    if(lua_getfield(L, 1, "remove_empty_query") == LUA_TBOOLEAN) {
      if (lua_toboolean(L, -1)) {
        flags |= URLFL_REMOVE_EMPTY_QUERY;
      }
    }
    lua_pop(L, 1);
    if(lua_getfield(L, 1, "remove_empty_fragment") == LUA_TBOOLEAN) {
      if (lua_toboolean(L, -1)) {
        flags |= URLFL_REMOVE_EMPTY_FRAGMENT;
      }
    }
    lua_pop(L, 1);
  }
  memset(&opts, 0, sizeof(opts));
  opts.flags = flags;
  ctx = lua_newuserdata(L, sizeof(url_ctx_t*));
  luaL_setmetatable(L, MTNAME_URLBUILDER);
  *ctx = url_ctx_new(&opts);
  if (*ctx == NULL) {
    return luaL_error(L, "URLBuilder: initialization error");
  }
  return 1;
}

static int l_urlbuilder_gc(lua_State *L) {
  url_ctx_t *ctx = checkurlbuilder(L, 1);
  url_ctx_free(ctx);
  return 0;
}

static int l_urlbuilder_parse(lua_State *L) {
  int ret;
  struct url_parts parts;
  url_ctx_t *ctx = checkurlbuilder(L, 1);
  const char *urlstr = luaL_checkstring(L, 2);
  const struct url_opts *opts = url_ctx_opts(ctx);

  if ((ret = url_parse(ctx, urlstr, &parts)) != EURL_OK) {
    return luaL_error(L, url_errstr(ret));
  }

  lua_newtable(L);
  if ((parts.schemelen) > 0) {
    lua_pushlstring(L, urlstr+parts.scheme, parts.schemelen);
    lua_setfield(L, -2, "scheme");
  }

  if ((parts.authlen) > 0) {
    lua_pushlstring(L, urlstr+parts.auth, parts.authlen);
    lua_setfield(L, -2, "auth");
  }

  if ((parts.flags & URLPART_HAS_USERINFO) != 0) {
    /* we may have a zero length userinfo, and for inverse symmetry between
     * parse and build we need to be able to represent this separately from
     * no userinfo present. That's why we check URLPART_HAS_USERINFO instead
     * of parts.userinfolen */
    lua_pushlstring(L, urlstr+parts.userinfo, parts.userinfolen);
    lua_setfield(L, -2, "userinfo");
  }

  if ((parts.hostlen) > 0) {
    lua_pushlstring(L, urlstr+parts.host, parts.hostlen);
    lua_setfield(L, -2, "host");
  }

  if ((parts.flags & URLPART_HAS_PORT) != 0) {
    /* see userinfo comment above on why we check URLPART_HAS_PORT*/
    lua_pushlstring(L, urlstr+parts.port, parts.portlen);
    lua_setfield(L, -2, "port");
  }

  if ((parts.pathlen) > 0) {
    lua_pushlstring(L, urlstr+parts.path, parts.pathlen);
    lua_setfield(L, -2, "path");
  }

  if ((parts.querylen) > 0) {
    lua_pushlstring(L, urlstr+parts.query, parts.querylen);
    lua_setfield(L, -2, "query");
  } else if ((opts->flags & URLFL_REMOVE_EMPTY_QUERY) == 0 &&
      parts.flags & URLPART_HAS_QUERY) {
    lua_pushstring(L, "");
    lua_setfield(L, -2, "query");
  }

  if ((parts.fragmentlen) > 0) {
    lua_pushlstring(L, urlstr+parts.fragment, parts.fragmentlen);
    lua_setfield(L, -2, "fragment");
  } else if ((opts->flags & URLFL_REMOVE_EMPTY_FRAGMENT) == 0 &&
      parts.flags & URLPART_HAS_FRAGMENT) {
    lua_pushstring(L, "");
    lua_setfield(L, -2, "fragment");
  }

  return 1;
}

static int l_urlbuilder_build(lua_State *L) {
  int ret, has_auth = 0;
  luaL_Buffer b;
  url_ctx_t *ctx = checkurlbuilder(L, 1);
  (void)ctx;
  luaL_checktype(L, 2, LUA_TTABLE);
  luaL_buffinit(L, &b);

  lua_getfield(L, 2, "scheme");
  if (lua_type(L, -1) != LUA_TNIL) {
    if ((ret = url_supported_scheme(lua_tostring(L, -1))) != EURL_OK) {
      return luaL_error(L, url_errstr(ret));
    }
    luaL_addvalue(&b);
    luaL_addstring(&b, ":");
  } else {
    lua_pop(L, 1);
  }

  lua_getfield(L, 2, "port");
  lua_getfield(L, 2, "host");
  lua_getfield(L, 2, "userinfo");
  if (lua_type(L, -2) != LUA_TNIL ||
      lua_type(L, -3) != LUA_TNIL ||
      lua_type(L, -1) != LUA_TNIL) {
    has_auth = 1;
    luaL_addstring(&b, "//");
    if (lua_type(L, -1) != LUA_TNIL) { /* userinfo */
      luaL_addvalue(&b);
      luaL_addchar(&b, '@');
    } else {
      lua_pop(L, 1);
    }
    if (lua_type(L, -1) != LUA_TNIL) { /* host */
      luaL_addvalue(&b);
    } else {
      lua_pop(L, 1);
    }
    if (lua_type(L, -1) != LUA_TNIL) { /* port */
      luaL_addchar(&b, ':');
      luaL_addvalue(&b);
    } else {
      lua_pop(L, 1);
    }
  } else {
    lua_pop(L, 3);
    lua_getfield(L, 2, "auth");
    if (lua_type(L, -1) != LUA_TNIL) {
      has_auth = 1;
      luaL_addstring(&b, "//");
      luaL_addvalue(&b);
    }
  }

  lua_getfield(L, 2, "path");
  if (lua_type(L, -1) != LUA_TNIL) {
    const char *s = lua_tostring(L, -1);
    if (*s != '/' && has_auth) {
      luaL_addchar(&b, '/');
    }
    luaL_addvalue(&b);
  } else {
    lua_pop(L, 1);
  }

  lua_getfield(L, 2, "query");
  if (lua_type(L, -1) != LUA_TNIL) {
    luaL_addchar(&b, '?');
    luaL_addvalue(&b);
  } else {
    lua_pop(L, 1);
  }

  lua_getfield(L, 2, "fragment");
  if (lua_type(L, -1) != LUA_TNIL) {
    luaL_addchar(&b, '#');
    luaL_addvalue(&b);
  } else {
    lua_pop(L, 1);
  }

  luaL_pushresult(&b);
  return 1;
}

static int l_urlbuilder_normalize(lua_State *L) {
  buf_t buf;
  int ret;
  url_ctx_t *ctx = checkurlbuilder(L, 1);
  const char *urlstr = luaL_checkstring(L, 2);
  buf_init(&buf, 1024);
  if ((ret = url_normalize(ctx, urlstr, &buf)) != EURL_OK) {
    buf_cleanup(&buf);
    return luaL_error(L, url_errstr(ret));
  }
  lua_pushlstring(L, buf.data, buf.len-1); /* len includes trailing \0 */
  buf_cleanup(&buf);
  return 1;
}

static int l_urlbuilder_resolve(lua_State *L) {
  buf_t buf;
  int ret;
  url_ctx_t *ctx = checkurlbuilder(L, 1);
  const char *basestr = luaL_checkstring(L, 2);
  const char *refstr = luaL_checkstring(L, 3);
  buf_init(&buf, 1024);
  if ((ret = url_resolve(ctx, basestr, refstr, &buf)) != EURL_OK) {
    buf_cleanup(&buf);
    return luaL_error(L, url_errstr(ret));
  }
  lua_pushlstring(L, buf.data, buf.len-1);
  buf_cleanup(&buf);
  return 1;
}

static const struct luaL_Reg urlbuilder_m[] = {
  {"parse", l_urlbuilder_parse},
  {"build", l_urlbuilder_build},
  {"normalize", l_urlbuilder_normalize},
  {"resolve", l_urlbuilder_resolve},
  {"__gc", l_urlbuilder_gc},
  {NULL, NULL}
};

static const struct luaL_Reg http_f[] = {
  {"url_builder", l_urlbuilder},
  {NULL, NULL}
};

int luaopen_http(lua_State *L) {
  size_t i;
  struct {
    const char *mt;
    const struct luaL_Reg *reg;
  } mt[] = {
    {MTNAME_URLBUILDER, urlbuilder_m},
    {NULL, NULL},
  };

  /* init metatable(s) */
  for(i=0; mt[i].mt != NULL; i++) {
    luaL_newmetatable(L, mt[i].mt);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, mt[i].reg, 0);
  }

  luaL_newlib(L, http_f);
  return 1;
}

