#include <jansson.h>
#include <lib/lua/json.h>

#define MTNAME_JSONT "json.T"

#define checkjson(L, i) \
    (*(json_t**)luaL_checkudata(L, (i), MTNAME_JSONT))

static inline int newjson(lua_State *L, json_t *root) {
  json_t **json;

  json = lua_newuserdata(L, sizeof(json_t*));
  luaL_setmetatable(L, MTNAME_JSONT);
  *json = root;
  return 1;
}

static int l_from_str(lua_State *L) {
  const char *s;
  json_t *root;
  json_error_t err;
  size_t slen;

  s = luaL_checklstring(L, 1, &slen);
  root = json_loadb(s, slen, JSON_REJECT_DUPLICATES, &err);
  if (root == NULL) {
    return luaL_error(L, "json: error on line %d: %s", err.line,
        err.text);
  }

  return newjson(L, root);
}

static int write_json(const char *buffer, size_t size, void *data) {
  luaL_Buffer *b = data;
  luaL_addlstring(b, buffer, size);
  return 0;
}

static int l_tostring(lua_State *L) {
  luaL_Buffer b;
  json_t *json = checkjson(L, 1);

  luaL_buffinit(L, &b);
  json_dump_callback(json, write_json, &b, JSON_COMPACT | JSON_ENSURE_ASCII);
  luaL_pushresult(&b);
  return 1;
}

static int l_gc(lua_State *L) {
  json_t *json = checkjson(L, 1);
  if (json != NULL) {
    json_decref(json);
  }
  return 0;
}

static int l_push_value(lua_State *L, json_t *val) {
  if (val == NULL) {
    lua_pushnil(L);
    return 1;
  }

  switch (json_typeof(val)) {
  case JSON_OBJECT: /* fall-through */
  case JSON_ARRAY:
    newjson(L, val);
    json_incref(val);
    break;
  case JSON_STRING:
    lua_pushlstring(L, json_string_value(val), json_string_length(val));
    break;
  case JSON_INTEGER:
    lua_pushinteger(L, json_integer_value(val));
    break;
  case JSON_REAL:
    lua_pushnumber(L, json_real_value(val));
    break;
  case JSON_TRUE:
    lua_pushboolean(L, 1);
    break;
  case JSON_FALSE:
    lua_pushboolean(L, 0);
    break;
  case JSON_NULL:
    lua_pushnil(L);
    break;
  default:
    return luaL_error(L, "unknown json value type: %d", json_typeof(val));
  }
  return 1;
}

static int l_index(lua_State *L) {
  int typ;
  json_t *json = checkjson(L, 1);
  json_t *val;
  size_t index;

  typ = json_typeof(json);
  if (typ == JSON_OBJECT) {
    if (!lua_isstring(L, 2)) {
      return luaL_error(L, "indexing of JSON object with a non-string key");
    }
    val = json_object_get(json, lua_tostring(L, 2));
    return l_push_value(L, val);
  } else if (typ == JSON_ARRAY) {
    if (!lua_isinteger(L, 2)) {
      return luaL_error(L, "indexing of JSON array with a non-integer key");
    }
    /* subtract 1 from index to conform with Lua's 1-indexing */
    index = (size_t)lua_tointeger(L, 2) - 1;
    val = json_array_get(json, index);
    return l_push_value(L, val);
  } else {
    lua_pushnil(L);
    return 1;
  }
}

static json_t *l_to_json(lua_State *L, int index) {
  int typ = lua_type(L, index);
  const char *v;
  size_t len;

  /* LUA_TTABLE is ambiguous and should not be converted */
  switch(typ) {
    case LUA_TNIL:
      return json_null();
    case LUA_TBOOLEAN:
      return lua_toboolean(L, index) ? json_true() : json_false();
    case LUA_TNUMBER:
      if (lua_isinteger(L, index)) {
        return json_integer(lua_tointeger(L, index));
      } else {
        return json_real(lua_tonumber(L, index));
      }
    case LUA_TSTRING:
      v = lua_tolstring(L, index, &len);
      return json_stringn(v, len);
    case LUA_TUSERDATA:
      return json_deep_copy(checkjson(L, index));
  }

  luaL_error(L, "invalid type in assignment: %s", lua_typename(L, typ));
  return NULL;
}

static int l_newindex(lua_State *L) {
  int typ;
  json_t *json = checkjson(L, 1);
  json_t *val;
  const char *strkey;
  size_t index;
  int ret;

  typ = json_typeof(json);
  if (typ == JSON_OBJECT) {
    if (!lua_isstring(L, 2)) {
      return luaL_error(L, "indexing of JSON object with a non-string key");
    }
    strkey = lua_tostring(L, 2);
    val = l_to_json(L, -1);
    ret = json_object_set_new(json, strkey, val);
    if (ret < 0) {
      return luaL_error(L, "set operation failed");
    }
  } else if (typ == JSON_ARRAY) {
    if (!lua_isinteger(L, 2)) {
      return luaL_error(L, "indexing of JSON array with a non-integer key");
    }
    /* subtract one from index to conform with Lua's 1-indexing */
    index = (size_t)lua_tointeger(L, 2) - 1;
    if (index > (json_array_size(json) - 1)) {
      return luaL_error(L, "array index out-of-bounds");
    }
    val = l_to_json(L, -1);
    ret = json_array_set_new(json, index, val);
    if (ret < 0) {
      return luaL_error(L, "set operation failed");
    }
  }

  return 0;
}

static int l_append(lua_State *L) {
  json_t *json = checkjson(L, 1);

  if (json_typeof(json) != JSON_ARRAY) {
    return luaL_error(L, "attempt to append to invalid JSON array");
  }

  json_array_append_new(json, l_to_json(L, 2));
  lua_pop(L, 1);
  return 1;
}

static const struct luaL_Reg jsont_m[] = {
  {"__tostring", l_tostring},
  {"__gc", l_gc},
  {"__index", l_index},
  {"__newindex", l_newindex},
  {"__shl", l_append},
  {NULL, NULL}
};

static const struct luaL_Reg json_f[] = {
  {"from_str", l_from_str},
  {NULL, NULL}
};

int luaopen_json(lua_State *L) {
  struct {
    const char *mt;
    const struct luaL_Reg *reg;
  } types[] = {
    {MTNAME_JSONT, jsont_m},
    {NULL, NULL},
  };
  size_t i;

  /* seed JSON hashtable, 0 == seed from /dev/[u]random, or time+pid */
  json_object_seed(0);

  /* create metatable(s) */
  for(i=0; types[i].mt != NULL; i++) {
    luaL_newmetatable(L, types[i].mt);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, types[i].reg, 0);
  }

  /* register library */
  luaL_newlib(L, json_f);
  return 1;
}
