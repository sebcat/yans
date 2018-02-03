#include <time.h>
#include <string.h>
#include <errno.h>
#include <lib/lua/util.h>

static int l_nanosleep(lua_State *L) {
  lua_Integer sec;
  lua_Integer nsec;
  struct timespec req;
  struct timespec remaining = {0};
  int ret;

  sec = luaL_checkinteger(L, 1);
  nsec = luaL_checkinteger(L, 2);
  req.tv_sec = (time_t)sec;
  req.tv_nsec = (long)nsec;
  while ((ret = nanosleep(&req, &remaining)) < 0 && errno == EINTR) {
    req = remaining;
  }

  if (ret < 0) {
    return luaL_error(L, "%s", strerror(errno));
  }

  return 0;
}

static const struct luaL_Reg util_f[] = {
  {"nanosleep", l_nanosleep},
  {NULL, NULL},
};

int luaopen_util(lua_State *L) {
  /* register util library functions */
  luaL_newlib(L, util_f);
  return 1;
}
