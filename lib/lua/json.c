#include <3rd_party/jansson.h>
#include <lib/lua/json.h>

/* TODO: 3rd_party/jansson has it's own hash table, string buffers and
 *       internal data structures. It would be nice if we could skip the
 *       step going from json_t to Lua values by integrating it tighter
 *       with Lua, if possible. */

/* maximum JSON nesting. Maybe too conservative, we'll see. Each level of JSON
 * nesting increases affects the Lua stack by at least a factor of three.
 * lua_next has table, key, value for every level. */
#define MAXDEPTH 32

/* calculates the length of the Lua array at TOS, or returns -1 if it's not
 * a valid lua array. Currently, if all keys are integers, we say it's an
 * "array" which doesn't really hold up, but sane alternatives appears to be
 * lacking */
static lua_Integer array_length(lua_State *L) {
  lua_Integer len = 0;

  if (!lua_istable(L, -1)) {
    return -1;
  }

  lua_pushnil(L);  /* first key */
  while (lua_next(L, -2) != 0) {
    /* uses 'key' (at index -2) and 'value' (at index -1) */
    if (!lua_isinteger(L, -2)) {
      lua_pop(L, 2);
      return -1;
    }

    len++;
    lua_pop(L, 1); /* pop the value */
  }

  return len;
}

static json_t *l_encode_json(lua_State *L, int depth);

/* XXX: This solution does not properly handle nil elements
 *      (they are not included). We *could* check the key indices and pad with
 *      JSON null's on gaps, but that could blow up in other ways. */
static json_t *l_encode_array(lua_State *L, int depth) {
  json_t *node;
  json_t *val;

  if (!lua_istable(L, -1)) {
    return NULL;
  }

  node = json_array();
  lua_pushnil(L);
  while (lua_next(L, -2) != 0) {
    val = l_encode_json(L, depth);
    if (val != NULL) {
      json_array_append_new(node, val);
    }
    lua_pop(L, 1);
  }

  return node;
}

static json_t *l_encode_object(lua_State *L, int depth) {
  json_t *node;
  json_t *val;
  const char *key;
  char buf[32];

  if (!lua_istable(L, -1)) {
    return NULL;
  }

  node = json_object();
  lua_pushnil(L);
  while (lua_next(L, -2) != 0) {
    if (lua_isstring(L, -2)) {
      key = lua_tostring(L, -2);
    } else if (lua_isnumber(L, -2)) {
      snprintf(buf, sizeof(buf), "%lld", (long long)lua_tonumber(L, -2));
      key = buf;
    } else {
      /* silent failure */
      lua_pop(L, 1);
      continue;
    }

    val = l_encode_json(L, depth);
    if (val != NULL) {
      json_object_set_new(node, key, val);
    }
    lua_pop(L, 1);
  }

  return node;
}

static json_t *l_encode_json(lua_State *L, int depth) {
  json_t *node = NULL;
  size_t len = 0;
  const char *str;
  lua_Integer arrlen;

  switch (lua_type(L, -1)) {
  case LUA_TNIL:
    node = json_null();
    break;
  case LUA_TNUMBER:
    if (lua_isinteger(L, -1)) {
      node = json_integer(lua_tointeger(L, -1));
    } else if (lua_isnumber(L, -1)) {
      node = json_real(lua_tonumber(L, -1));
    }
    break;
  case LUA_TBOOLEAN:
    node = json_boolean(lua_toboolean(L, -1));
    break;
  case LUA_TSTRING:
    str = lua_tolstring(L, -1, &len);
    node = json_stringn_nocheck(str, len);
    break;
  case LUA_TTABLE:
    if (depth >= MAXDEPTH) {
      /* XXX: node memleak, pass root along and call json_decref on it */
      luaL_error(L, "json: nested too deeply (maxdepth: %d)", MAXDEPTH);
    }
    arrlen = array_length(L);
    if (arrlen < 0) {
      /* treat table as JSON object */
      node = l_encode_object(L, depth + 1);
    } else if (arrlen == 0) {
      /* we have an empty table, we don't know if it represents a JSON
       * object or a JSON array. Set it to null */
      node = json_null();
    } else {
      node = l_encode_array(L, depth + 1);
    }
    break;
  default:
    /* XXX: node memleak, pass root along and call json_decref on it */
    luaL_error(L, "invalid type: %s", lua_typename(L, 1));
  }

  return node;
}

static int write_json(const char *buffer, size_t size, void *data) {
  luaL_Buffer *b = data;
  luaL_addlstring(b, buffer, size);
  return 0;
}

static int l_encode(lua_State *L) {
  json_t *node = NULL;
  int ret;
  luaL_Buffer b;

  node = l_encode_json(L, 0);
  if (node != NULL) {
    luaL_buffinit(L, &b);
    ret = json_dump_callback(node, write_json, &b, JSON_COMPACT |
        JSON_ENCODE_ANY | JSON_ENSURE_ASCII);
    luaL_pushresult(&b);
    json_decref(node);
    if (ret == 0) {
      return 1;
    }
  }
  return 0;
}

static int l_decode_json(lua_State *L, json_t *node, int depth);

static int l_decode_object(lua_State *L, json_t *node, int depth) {
  const char *key;
  json_t *value;

  lua_createtable(L, 0, json_object_size(node));
  json_object_foreach(node, key, value) {
    l_decode_json(L, value, depth);
    lua_setfield(L, -2, key);
  }

  return 1;
}

static int l_decode_array(lua_State *L, json_t *node, int depth) {
  size_t index;
  json_t *value;

  lua_createtable(L, json_array_size(node), 0);
  json_array_foreach(node, index, value) {
    l_decode_json(L, value, depth);
    lua_seti(L, -2, (lua_Integer)index+1);
  }

  return 1;
}

static int l_decode_json(lua_State *L, json_t *node, int depth) {
  switch(json_typeof(node)) {
  case JSON_OBJECT:
    if (depth >= MAXDEPTH) {
      /* XXX: node memleak, pass root along and call json_decref on it */
      luaL_error(L, "json: nested too deeply (maxdepth: %d)", MAXDEPTH);
    }
    l_decode_object(L, node, depth + 1);
    break;
  case JSON_ARRAY:
    if (depth >= MAXDEPTH) {
      /* XXX: node memleak, pass root along and call json_decref on it */
      luaL_error(L, "json: nested too deeply (maxdepth: %d)", MAXDEPTH);
    }
    l_decode_array(L, node, depth + 1);
    break;
  case JSON_STRING:
    lua_pushlstring(L, json_string_value(node), json_string_length(node));
    break;
  case JSON_INTEGER:
    lua_pushinteger(L, (lua_Integer)json_integer_value(node));
    break;
  case JSON_REAL:
    lua_pushnumber(L, (lua_Number)json_real_value(node));
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
    /* XXX: node memleak, pass root along and call json_decref on it */
    return luaL_error(L, "invalid JSON type (%d)", json_typeof(node));
  }

  return 1;
}

static int l_decode(lua_State *L) {
  const char *s;
  json_t *root;
  json_error_t err;
  size_t slen;

  s = luaL_checklstring(L, 1, &slen);
  root = json_loadb(s, slen, JSON_REJECT_DUPLICATES | JSON_DECODE_ANY |
      JSON_DISABLE_EOF_CHECK | JSON_ALLOW_NUL, &err);
  if (root == NULL) {
    return luaL_error(L, "json: error on line %d: %s\n", err.line,
        err.text);
  }

  l_decode_json(L, root, 0);
  json_decref(root);
  /* TODO: return trailing data, e.g., {"foo":"bar"}{"k":"v"} should return
           {"k":"v"} as the first object as a Lua table and the second object
           as a string. This allows for streaming objects from a buffer */
  return 1;
}

static const struct luaL_Reg json_f[] = {
  {"encode", l_encode},
  {"decode", l_decode},
  {NULL, NULL}
};

int luaopen_json(lua_State *L) {
  json_object_seed(0); /* 0 == seed from /dev/[u]random, or time+pid */
  luaL_newlib(L, json_f);
  return 1;
}
