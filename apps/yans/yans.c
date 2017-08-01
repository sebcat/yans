/* vim: set tabstop=2 shiftwidth=2 expandtab ai: */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <3rd_party/linenoise.h>
#include <3rd_party/lua.h>
#include <3rd_party/lpeg.h>

#include <lib/lua/net.h>
#include <lib/lua/http.h>
#include <lib/lua/ypcap.h>
#include <lib/lua/pcapd.h>
#include <lib/lua/json.h>

#define YREPL_HISTORY 2000
#define YREPL_PROMPT  ">>> "
#define YREPL_CONTP   "    "
#define YREPL_SHIST   "/s "

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

static lua_State * state_or_die() {
  lua_State *L;

  L = luaL_newstate();
  if (L == NULL) {
    fprintf(stderr, "unable to create Lua state\n");
    exit(EXIT_FAILURE);
  }

  luaL_openlibs(L);
  luaL_requiref(L, "ip", luaopen_ip, 1);
  luaL_requiref(L, "eth", luaopen_eth, 1);
  luaL_requiref(L, "http", luaopen_http, 1);
  luaL_requiref(L, "ypcap", luaopen_ypcap, 1);
  luaL_requiref(L, "pcapd", luaopen_pcapd, 1);
  luaL_requiref(L, "lpeg", luaopen_lpeg, 1);
  luaL_requiref(L, "json", luaopen_json, 1);
  return L;
}

static int cmd_shell(int argc, char *argv[]) {
  lua_State *L;

  L = state_or_die();
  linenoiseSetMultiLine(1);
  linenoiseHistorySetMaxLen(YREPL_HISTORY);
  if (isatty(0)) {
    repl(L);
  } else {
    /*eval stdin as file */
  }
  lua_close(L);
  return EXIT_SUCCESS;
}

static int evalfile(const char *filename) {
  lua_State *L;
  int exitcode = EXIT_FAILURE;

  L = state_or_die();
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

void usage() {
  fprintf(stderr,
    "options:\n"
    "  shell    - start the shell\n");
  exit(EXIT_FAILURE);
}

typedef int(*cmdfunc)(int, char **);

typedef struct {
  const char *name;
  cmdfunc func;
} cmdfunc_t;

int main(int argc, char *argv[]) {
  int i;
  int exitcode = EXIT_FAILURE;
  static const cmdfunc_t cmds[] = {
    {"shell", cmd_shell},

  };

  signal(SIGPIPE, SIG_IGN);

  if (argc == 1) {
    /* default to REPL */
    return cmd_shell(argc-1, argv+1);
  } else if (argv[1][0] == '-' &&argv[1][1] == 'h') {
    /* check -h */
    usage();
  }

  for(i=0; i<sizeof(cmds)/sizeof(cmdfunc_t); i++) {
    if (strcmp(argv[1], cmds[i].name) == 0) {
      return cmds[i].func(argc-2, argv+2);
    }
  }

  /* not a command, is it a file? */
  if (argv[1][0] == '-' && argv[1][1] == '\0') {
    exitcode = evalfile(NULL);
  } else {
    exitcode = evalfile(argv[1]);
  }

  return exitcode;
}
