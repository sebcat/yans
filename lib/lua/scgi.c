#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <lib/net/scgi.h>
#include <lib/lua/scgi.h>

#define MIN(a__,b__) ((a__) <= (b__) ? (a__) : (b__))

#define BODY_SIZE_LIMIT 10485760

static int l_read_request(lua_State *L) {
  struct scgi_ctx ctx = {0};
  struct scgi_header hdr = {0};
  int ret;
  int t;
  lua_Integer clen = 0;

  ret = scgi_init(&ctx, STDIN_FILENO, SCGI_DEFAULT_MAXHDRSZ);
  if (ret != SCGI_OK) {
    return luaL_error(L, "scgi_init: %s", scgi_strerror(ret));
  }

  while ((ret = scgi_read_header(&ctx)) == SCGI_AGAIN);
  if (ret != SCGI_OK) {
    scgi_cleanup(&ctx);
    return luaL_error(L, "scgi_read_header: %s", scgi_strerror(ret));
  }

  ret = scgi_parse_header(&ctx);
  if (ret != SCGI_OK) {
    scgi_cleanup(&ctx);
    return luaL_error(L, "scgi_parse_header: %s", scgi_strerror(ret));
  }

  /* set up the request headers as a table on the Lua parameter stack */
  lua_newtable(L);
  while ((ret = scgi_get_next_header(&ctx, &hdr)) == SCGI_AGAIN) {
    lua_pushlstring(L, hdr.value, hdr.valuelen);
    lua_setfield(L, -2, hdr.key);
  }
  if (ret != SCGI_OK) {
    scgi_cleanup(&ctx);
    return luaL_error(L, "scgi_get_next_header: %s", scgi_strerror(ret));
  }

  /* Do we have a request body? */
  t = lua_getfield(L, -1, "CONTENT_LENGTH");
  if (t == LUA_TSTRING && (clen = lua_tointeger(L, -1)) > 0) {
    const char *cptr;
    size_t restlen = 0;
    char buf[512];
    luaL_Buffer b;
    ssize_t nread;

    if (clen > BODY_SIZE_LIMIT) {
      scgi_cleanup(&ctx);
      return luaL_error(L, "request body exceeds %d bytes", BODY_SIZE_LIMIT);
    }

    luaL_buffinit(L, &b);
    /* load the scgi buffer rest, if any */
    cptr = scgi_get_rest(&ctx, &restlen);
    if (cptr != NULL && restlen > 0) {
      luaL_addlstring(&b, cptr, restlen);
      assert(restlen <= clen);
      clen -= restlen;
    }
    /* read the rest of the body from stdin */
    while (clen > 0) {
      nread = read(STDIN_FILENO, buf, MIN(sizeof(buf), clen));
      if (nread < 0) {
        if (errno == EINTR) {
          continue;
        } else {
          scgi_cleanup(&ctx);
          return luaL_error(L, "read: %s", strerror(errno));
        }
      } else if (nread == 0) {
        scgi_cleanup(&ctx);
        return luaL_error(L, "short read on request body");
      } else {
        luaL_addlstring(&b, buf, (size_t)nread);
        clen -= nread;
      }
    }
    luaL_pushresult(&b);
    lua_rotate(L, -2, 1);
    lua_pop(L, 1);
  } else {
    /* no/invalid CONTENT_LENGTH, or CONTENT_LENGTH: 0 */
    lua_pop(L, 1);
    lua_pushnil(L);
  }

  return 2;
}

static const struct luaL_Reg scgi_f[] = {
  {"read_request", l_read_request},
  {NULL, NULL},
};

int luaopen_scgi(lua_State *L) {
  luaL_newlib(L, scgi_f);
  return 1;
}
