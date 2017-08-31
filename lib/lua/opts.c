#include <string.h>
#include <stdlib.h>

#include <lib/lua/opts.h>

#define MTNAME_OPTS "yans.Opts"

#define OPTS_ERRMEM       -1
#define OPTS_ERRDUPSHORT  -2
#define OPTS_ERRDUPLONG   -3

#define checkopts(L, i) \
    ((struct opts*)luaL_checkudata(L, (i), MTNAME_OPTS))

enum opt_type {
  OPTT_FLAG,
  OPTT_STR,
  OPTT_INT,
};

struct opts {
  size_t nopts;
  size_t cap;
  enum opt_type *types;
  char **longnames;
  char *shortnames;
  char **descs;
};

struct optsargs {
  const char *longname;
  const char *desc;
  char shortname;
};

static int opts_findshortname(struct opts *opts, char shortname) {
  int i;
  for (i = 0; i < opts->nopts; i++) {
    if (opts->shortnames[i] == shortname) {
      return i;
    }
  }
  return -1;
}

static int opts_findlongname(struct opts *opts, const char *longname) {
  int i;
  for (i = 0; i < opts->nopts; i++) {
    if (strcmp(opts->longnames[i], longname) == 0) {
      return i;
    }
  }
  return -1;
}

static const char *opts_typestr(enum opt_type t) {
  switch (t) {
  case OPTT_FLAG:
    return "flag";
  case OPTT_STR:
    return "str";
  case OPTT_INT:
    return "int";
  default:
    return "t:???";
  }
}

static void opts_cleanup(struct opts *opts) {
  size_t i;

  if (opts != NULL) {
    if (opts->nopts > 0) {
      for (i = 0; i < opts->nopts; i++) {
        free(opts->longnames[i]);
        free(opts->descs[i]);
      }

      free(opts->types);
      free(opts->longnames);
      free(opts->shortnames);
      free(opts->descs);
    }
  }
}

static int opts_grow(struct opts *opts) {
  size_t newcap;
  enum opt_type *newtypes = NULL;
  char **newlongnames = NULL;
  char *newshortnames = NULL;
  char **newdescs = NULL;

  /* don't grow if we don't need to */
  if (opts->nopts < opts->cap) {
    return 0;
  }

  /* calculate new capacity */
  newcap = (opts->cap == 0) ? 10 : opts->cap + opts->cap/2;

  /* (re-)allocate memory */
  if ((newtypes = realloc(opts->types, newcap * sizeof(*newtypes))) == NULL) {
    return OPTS_ERRMEM;
  }
  opts->types = newtypes;
  if ((newlongnames = realloc(opts->longnames,
      newcap * sizeof(*newlongnames))) == NULL) {
    return OPTS_ERRMEM;
  }
  opts->longnames = newlongnames;
  if ((newshortnames = realloc(opts->shortnames,
      newcap * sizeof(*newshortnames))) == NULL) {
    return OPTS_ERRMEM;
  }
  opts->shortnames = newshortnames;
  if ((newdescs = realloc(opts->descs, newcap * sizeof(*newdescs))) == NULL) {
    return OPTS_ERRMEM;
  }
  opts->descs = newdescs;
  opts->cap = newcap;
  return 0;
}

static int _opts_append(struct opts *opts, enum opt_type t,
    struct optsargs *args) {
  int ret;
  char *newlongname;
  char *newdesc;

  /* grow if needed */
  if ((ret = opts_grow(opts)) < 0) {
    return ret;
  }

  /* allocate and copy strings */
  newlongname = strdup(args->longname);
  newdesc = strdup(args->desc);
  if (newlongname == NULL) {
    return OPTS_ERRMEM;
  } else if (newdesc == NULL) {
    free(newlongname);
    return OPTS_ERRMEM;
  }

  /* assign values and increment nopts */
  opts->types[opts->nopts] = t;
  opts->longnames[opts->nopts] = newlongname;
  opts->shortnames[opts->nopts] = args->shortname;
  opts->descs[opts->nopts] = newdesc;
  opts->nopts++;
  return 0;
}

static int opts_append(struct opts *opts, enum opt_type t,
    struct optsargs *args) {
  size_t i;

  /* check if unique, return error if not */
  for (i = 0; i < opts->nopts; i++) {
    if (strcmp(opts->longnames[i], args->longname) == 0) {
      return OPTS_ERRDUPLONG;
    } else if (opts->shortnames[i] == args->shortname) {
      return OPTS_ERRDUPSHORT;
    }
  }

  return _opts_append(opts, t, args);
}

static const char *opts_strerror(int code) {
  switch (code) {
  case OPTS_ERRMEM:
    return "opts: memory allocation error";
  case OPTS_ERRDUPSHORT:
    return "opts: duplicate short name";
  case OPTS_ERRDUPLONG:
    return "opts: duplicate long name";
  default:
    return "opts: unknown error";
  }
}

static int l_optsgc(lua_State *L) {
  struct opts *opts = checkopts(L, 1);
  opts_cleanup(opts);
  return 0;
}

static int l_newopts(lua_State *L) {
  struct opts *opts;
  opts = lua_newuserdata(L, sizeof(struct opts));
  luaL_setmetatable(L, MTNAME_OPTS);
  memset(opts, 0, sizeof(*opts));
  return 1;
}

static void l_optsargs(lua_State *L, enum opt_type t) {
  int ret;
  const char *tmp;
  struct optsargs args = {0};
  struct opts *opts = checkopts(L, 1);
  args.longname = luaL_checkstring(L, 2);
  tmp = luaL_checkstring(L, 3);
  if (tmp[0] == '\0') {
    luaL_error(L, "empty short option");
  }
  args.shortname = tmp[0];
  args.desc = luaL_checkstring(L, 4);
  if ((ret = opts_append(opts, t, &args)) < 0) {
    luaL_error(L, "%s", opts_strerror(ret));
  }
}

static int l_optsflag(lua_State *L) {
  l_optsargs(L, OPTT_FLAG);
  return 0;
}

static int l_optsint(lua_State *L) {
  l_optsargs(L, OPTT_INT);
  return 0;
}

static int l_optsstr(lua_State *L) {
  l_optsargs(L, OPTT_STR);
  return 0;
}

static int l_optsparse(lua_State *L) {
  struct opts *opts = checkopts(L, 1);
  size_t i;
  size_t len;
  const char *str;
  int index;
  int isnum;

  luaL_checktype(L, 2, LUA_TTABLE);
  len = lua_rawlen(L, 2);
  lua_newtable(L);
  for (i = 1; i <= len; i++) {
    lua_rawgeti(L, 2, i);
    str = lua_tostring(L, -1);
    if (str == NULL) {
      lua_pop(L, 1);
      continue;
    } else if (str[0] == '-' && str[1] != '-' && str[1] != '\0') {
      /* shortname */
      index = opts_findshortname(opts, str[1]);
      if (index == -1) {
        return luaL_error(L, "invalid flag: %s", str);
      }
    } else if (str[0] == '-' && str[1] == '-') {
      /* longname */
      index = opts_findlongname(opts, str + 2);
      if (index == -1) {
        return luaL_error(L, "invalid flag: %s", str);
      }
    } else {
      /* non-option */
      lua_pop(L, 1);
      break;
    }

    lua_pop(L, 1); /* remove flag */
    if (opts->types[index] == OPTT_FLAG) {
      lua_pushboolean(L, 1);
      lua_setfield(L, 3, opts->longnames[index]);
      continue;
    }

    if (i == len) {
      return luaL_error(L, "missing value for flag: %s",
          opts->longnames[index]);
    }
    i++;
    lua_rawgeti(L, 2, i); /* push value */
    if (opts->types[index] == OPTT_STR) {
      lua_setfield(L, 3, opts->longnames[index]);
    } else {
      lua_tointegerx(L, -1, &isnum);
      if (!isnum) {
        return luaL_error(L, "non-integer value for %s",
            opts->longnames[index]);
      }
      lua_setfield(L, 3, opts->longnames[index]);
    }
  }

  lua_remove(L, 2);
  lua_pushinteger(L, (lua_Integer)i);
  return 2;
}

static int l_optsusage(lua_State *L) {
  luaL_Buffer b;
  size_t maxlen = 0;
  size_t tmp;
  size_t i;
  char buf[80]; /* limit to 80 chars */
  struct opts *opts = checkopts(L, 1);

  for (i = 0; i < opts->nopts; i++) {
    tmp = strlen(opts->longnames[i]);
    if (tmp > maxlen) {
      maxlen = tmp;
    }
  }

  luaL_buffinit(L, &b);
  luaL_addstring(&b, "options:\n");
  for (i = 0; i < opts->nopts; i++) {
    snprintf(buf, sizeof(buf), "  -%c, --%s: %s (%s)\n", opts->shortnames[i],
        opts->longnames[i], opts->descs[i], opts_typestr(opts->types[i]));
    luaL_addstring(&b, buf);
  }
  luaL_pushresult(&b);
  return 1;
}

/* opts meta-table */
static const struct luaL_Reg opts_m[] = {
  {"__gc", l_optsgc},
  {"flag", l_optsflag},
  {"int", l_optsint},
  {"str", l_optsstr},
  {"parse", l_optsparse},
  {"usage", l_optsusage},
  {NULL, NULL},
};

/* opts functions */
static const struct luaL_Reg opts_f[] = {
  {"new", l_newopts},
  {NULL, NULL},
};

int luaopen_opts(lua_State *L) {
  /* register opts meta-table */
  luaL_newmetatable(L, MTNAME_OPTS);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, opts_m, 0);

  /* register opts library functions */
  luaL_newlib(L, opts_f);
  return 1;
}
