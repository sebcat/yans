#include <time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <lib/util/os.h>
#include <lib/util/sandbox.h>
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

static int l_sandbox(lua_State *L) {

  if (sandbox_enter() < 0) {
    if (errno != 0) {
      return luaL_error(L, "sandbox_enter: %s", strerror(errno));
    } else {
      return luaL_error(L, "sandbox_enter: unknown error");
    }
  }

  return 0;
}

static int l_chdir(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  if (chdir(path) < 0) {
    return luaL_error(L, "chdir %s: %s", path, strerror(errno));
  }
  return 0;
}

static int l_daemonize(lua_State *L) {
  const char *user, *group;
  struct os_daemon_opts opts = {0};
  os_t os = {{0}};
  int ret;

  opts.name = luaL_checkstring(L, 1);
  opts.path = luaL_checkstring(L, 2);
  user = luaL_checkstring(L, 3);
  group = luaL_checkstring(L, 4);

  ret = os_getuid(&os, user, &opts.uid);
  if (ret != OS_OK) {
    return luaL_error(L, "os_getuid: %s", os_strerror(&os));
  }

  ret = os_getgid(&os, group, &opts.gid);
  if (ret != OS_OK) {
    return luaL_error(L, "os_getgid: %s", os_strerror(&os));
  }

  ret = os_daemonize(&os, &opts);
  if (ret != OS_OK) {
    return luaL_error(L, "os_daemonize: %s", os_strerror(&os));
  }

  return 0;
}

static int l_daemon_remove_pidfile(lua_State *L) {
  struct os_daemon_opts opts = {0};
  os_t os = {{0}};
  int ret;

  opts.name = luaL_checkstring(L, 1);
  opts.path = luaL_checkstring(L, 2);
  ret = os_daemon_remove_pidfile(&os, &opts);
  if (ret != OS_OK) {
    return luaL_error(L, "%s", os_strerror(&os));
  }

  return 0;
}

static const struct luaL_Reg util_f[] = {
  {"nanosleep", l_nanosleep},
  {"sandbox", l_sandbox},
  {"chdir", l_chdir},
  {"daemonize", l_daemonize},
  {"daemon_remove_pidfile", l_daemon_remove_pidfile},
  {NULL, NULL},
};

int luaopen_util(lua_State *L) {
  /* register util library functions */
  luaL_newlib(L, util_f);
  return 1;
}
