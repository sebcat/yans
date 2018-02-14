#include <lib/lua/file.h>
#include <fts.h>
#include <string.h>
#include <errno.h>

#define MTNAME_FTS "file.FTS"
#define DFL_MAXDEPTH 128

#define checkfts(L, i) \
    ((struct fts_data *)luaL_checkudata((L), (i), MTNAME_FTS))

struct fts_data {
  FTS *fts;
  lua_Integer maxdepth;
};

static int l_walk_iter(lua_State *L) {
  struct fts_data *data;
  FTSENT *ent;

  data = checkfts(L, lua_upvalueindex(1));
  errno = 0;
  ent = fts_read(data->fts);
  if (ent == NULL) {
    if (errno != 0) {
      return luaL_error(L, "fts_read: %s", strerror(errno));
    } else {
      return 0;
    }
  }

  if (ent->fts_level > data->maxdepth) {
    fts_set(data->fts, ent, FTS_SKIP);
  }

  lua_pushinteger(L, ent->fts_info);
  lua_pushstring(L, ent->fts_path);
  lua_pushinteger(L, ent->fts_level);
  return 3;
}

static int l_fts(lua_State *L) {
  struct fts_data *data;
  const char *path[2];
  lua_Integer maxdepth;
  int isnum = 0;

  path[1] = NULL;
  path[0] = luaL_checkstring(L, 1);
  maxdepth = lua_tointegerx(L, 2, &isnum);
  if (!isnum) {
    maxdepth = DFL_MAXDEPTH;
  }

  data = (struct fts_data*)lua_newuserdata(L, sizeof(struct fts_data));
  data->fts = NULL;
  luaL_getmetatable(L, MTNAME_FTS);
  lua_setmetatable(L, -2);
  data->maxdepth = maxdepth;
  data->fts = fts_open((char * const *)path,
      FTS_PHYSICAL | FTS_NOCHDIR, NULL);
  if (data->fts == NULL) {
    return luaL_error(L, "fts_open: %s", strerror(errno));
  }

  lua_pushcclosure(L, l_walk_iter, 1);
  return 1;
}

static int l_ftsgc(lua_State *L) {
  struct fts_data *data;

  data = checkfts(L, 1);
  if (data->fts != NULL) {
    fts_close(data->fts);
  }
  return 0;
}

static const struct luaL_Reg fts_f[] = {
  {"fts", l_fts},
  {NULL, NULL}
};

static const struct luaL_Reg fts_t[] = {
  {"__gc", l_ftsgc},
  {NULL, NULL},
};

static const struct {
  const char *name;
  lua_Integer val;
} g_intconsts[] = {
  {"FTS_D", FTS_D},
  {"FTS_DC", FTS_DC},
  {"FTS_DEFAULT", FTS_DEFAULT},
  {"FTS_DNR", FTS_DNR},
  {"FTS_DOT",FTS_DOT},
  {"FTS_DP", FTS_DP},
  {"FTS_ERR", FTS_ERR},
  {"FTS_F", FTS_F},
  {"FTS_NS", FTS_NS},
  {"FTS_NSOK", FTS_NSOK},
  {"FTS_SL", FTS_SL},
  {"FTS_SLNONE", FTS_SLNONE},
  {NULL, 0},
};

int luaopen_file(lua_State *L) {
  int i;

  luaL_newmetatable(L, MTNAME_FTS);
  luaL_setfuncs(L, fts_t, 0);
  luaL_newlib(L, fts_f);
  for (i = 0; g_intconsts[i].name != NULL; i++) {
    lua_pushinteger(L, g_intconsts[i].val);
    lua_setfield(L, -2, g_intconsts[i].name);
  }

  return 1;
}