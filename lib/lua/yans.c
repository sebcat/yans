#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <3rd_party/linenoise.h>
#include <3rd_party/lua.h>
#include <3rd_party/lpeg.h>

#include <lib/lua/net.h>
#include <lib/lua/http.h>
#include <lib/lua/json.h>
#include <lib/lua/cgi.h>
#include <lib/lua/fts.h>
#include <lib/lua/opts.h>
#include <lib/lua/ylog.h>
#include <lib/lua/eds.h>
#include <lib/lua/ycl.h>
#include <lib/lua/util.h>
#include <lib/lua/yans.h>

#define YREPL_HISTORY 2000
#define YREPL_PROMPT  ">>> "
#define YREPL_CONTP   "    "
#define YREPL_SHIST   "/s "

#ifndef DATAROOTDIR
#define DATAROOTDIR "/666noexist"
#endif

#ifdef YANS_DEBUG
#define YANS_PATH "./lib/yans/?.yans;./lib/yans/?/init.yans"
#else
#ifndef YANS_PATH
#define YANS_PATH DATAROOTDIR "/yans/?.yans;" \
    DATAROOTDIR "/yans/?/init.yans;" \
    "./?.yans;./?/init.yans"
#endif
#endif


static void repl_print(lua_State *L, int nkeep) {
  int nelems = lua_gettop(L)-nkeep;
  if (nelems > 0) {
    lua_getglobal(L, "print");
    lua_insert(L, 1);
    if (lua_pcall(L, nelems, 0, 0) != LUA_OK) {
      fprintf(stderr, "print error: %s\n", lua_tostring(L, -1));
    }
  }
}

static void repl_stmt(lua_State *L) {
  const char *str;
  char *line;
  size_t len;
  int ret;

  /* TOS: partial(?) statement */

eval_tos:
  /* try to load the statement */
  str = lua_tolstring(L, 1, &len);
  if ((ret = luaL_loadbuffer(L, str, len, "=stdin")) == LUA_OK) {
    if (lua_pcall(L, 0, LUA_MULTRET, 0) == LUA_OK) {
      repl_print(L, 1);
    } else {
      fprintf(stderr, "eval error: %s\n", lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  } else if (ret == LUA_ERRSYNTAX) {
    if (strstr(lua_tostring(L, -1), "<eof>") != NULL) {
      /* partial statement, get next line and continue */
      lua_pop(L, 1);
      line = linenoise(YREPL_CONTP);
      lua_pushliteral(L, "\n");
      lua_pushstring(L, line);
      free(line);
      lua_concat(L, 3);
      goto eval_tos;
    } else {
      fprintf(stderr, "syntax error: %s\n", lua_tostring(L, -1));
      lua_pop(L, 1); /* remove error message, keep string */
    }
  }
  /* TOS: what we evaled */
}

static void repl(lua_State *L) {
  char *line;
  int status;
  const char *retline;

  while((line = linenoise(YREPL_PROMPT)) != NULL) {
    lua_settop(L, 0);
    if (line[0] != '\0' && line[0] != '/') {
      /* prepend return and try to load the line as an expression */
      retline = lua_pushfstring(L, "return %s;", line);
      status = luaL_loadbuffer(L, retline, strlen(retline), "=stdin");
      if (status == LUA_OK) {
        lua_remove(L, -2); /* Remove eval-ed line */
        linenoiseHistoryAdd(line);
        if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
          fprintf(stderr, "eval error: %s\n", lua_tostring(L, -1));
          lua_pop(L, 1);
        } else {
          repl_print(L, 0);
        }
      } else {
        lua_pop(L, 2);
        lua_pushstring(L, line); /* push partial(?) statement */
        free(line);
        repl_stmt(L);
        linenoiseHistoryAdd(lua_tostring(L, -1));
        lua_pop(L, 1);
        continue;
      }


    } else if (strncmp(line, YREPL_SHIST, sizeof(YREPL_SHIST)-1)==0) {
      linenoiseHistorySave(line+sizeof(YREPL_SHIST)-1);
    }
    free(line);
  }
}

static lua_State *create_yans_state(const char *arg0, int argc, char **argv) {
  lua_State *L;
  int i;
  int t;
  static const struct {
    const char *name;
    int (*openfunc)(lua_State *);
  } libs[] = {
    {"ip", luaopen_ip},
    {"eth", luaopen_eth},
    {"http", luaopen_http},
    {"lpeg", luaopen_lpeg},
    {"json", luaopen_json},
    {"cgi", luaopen_cgi},
    {"fts", luaopen_fts},
    {"opts", luaopen_opts},
    {"ylog", luaopen_ylog},
    {"eds", luaopen_eds},
    {"ycl", luaopen_ycl},
    {"util", luaopen_util},
    {NULL, NULL},
  };

  L = luaL_newstate();
  if (L == NULL) {
    fprintf(stderr, "unable to create yans state\n");
    exit(EXIT_FAILURE);
  }

  if (arg0 != NULL) {
    lua_pushstring(L, arg0);
    lua_setglobal(L, "arg0");
  }

  lua_newtable(L);
  for (i = 0; i < argc; i++) {
    lua_pushstring(L, argv[i]);
    lua_rawseti(L, 1, (lua_Integer)i + 1);
  }
  lua_setglobal(L, "args");

  luaL_openlibs(L);
  for (i = 0; libs[i].name != NULL; i++) {
    luaL_requiref(L, libs[i].name, libs[i].openfunc, 1);
  }

  /* setup package.path */
  t = lua_getglobal(L, "package");
  if (t == LUA_TTABLE) {
    lua_pushstring(L, YANS_PATH);
    lua_setfield(L, -2, "path");
  }
  lua_pop(L, 1);

  return L;
}

int yans_evalfile(const char *filename, const char *arg0, int argc,
    char **argv) {
  lua_State *L;
  int exitcode = EXIT_FAILURE;

  L = create_yans_state(arg0, argc, argv);
  if (L == NULL) {
    fprintf(stderr, "unable to create yans state\n");
    return EXIT_FAILURE;
  }

  if (luaL_loadfile(L, filename) != LUA_OK) {
    fprintf(stderr, "%s\n", lua_tostring(L, -1));
    goto fail;
  }

  if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
    fprintf(stderr, "%s\n", lua_tostring(L, -1));
    goto fail;
  }

  exitcode = EXIT_SUCCESS;
fail:
  lua_close(L);
  return exitcode;
}

int yans_shell(const char *arg0, int argc, char *argv[]) {
  lua_State *L;

  L = create_yans_state(arg0, argc, argv);
  if (L == NULL) {
    fprintf(stderr, "unable to create yans state\n");
    return EXIT_FAILURE;
  }

  linenoiseSetMultiLine(1);
  linenoiseHistorySetMaxLen(YREPL_HISTORY);
  repl(L);
  lua_close(L);
  return EXIT_SUCCESS;
}

