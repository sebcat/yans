#include <string.h>
#include <stdlib.h>

#include <lib/util/buf.h>
#include <lib/util/sandbox.h>
#include <lib/lua/cgi.h>

extern char **environ;

static int l_init(lua_State *L) {
  char **curr;
  char *key;
  char *val;
  size_t keylen;
  buf_t buf;

  if (sandbox_enter() < 0) {
    return luaL_error(L, "Unable to enter sandbox");
  }

  lua_newtable(L);
  if (buf_init(&buf, 2048) == NULL) {
    return luaL_error(L, "memory allocation error");
  }

  for (curr = environ; curr != NULL && *curr != NULL; curr++) {
    key = *curr;
    /* remove fc2 prefix, if any */
    if (strncmp(key, "FC2_", 4) == 0) {
      key += 4;
    }

    val = strchr(key, '=');
    if (val == NULL) {
      continue;
    }
    keylen = val - key;
    val++;

    buf_adata(&buf, key, keylen);
    buf_achar(&buf, '\0');
    lua_pushstring(L, val);
    lua_setfield(L, -2, buf.data);
    buf_clear(&buf);
  }

  buf_cleanup(&buf);
  return 1;
}

static const struct luaL_Reg cgi_f[] = {
  {"init", l_init},
  {NULL, NULL}
};

int luaopen_cgi(lua_State *L) {
  luaL_newlib(L, cgi_f);
  return 1;
}
