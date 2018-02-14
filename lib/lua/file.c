#include <fts.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

#include <lib/lua/file.h>
#include <lib/util/zfile.h>

#define MTNAME_FTS "file.FTS"
#define DFL_MAXDEPTH 128

#define checkfts(L, i) \
    ((struct fts_data *)luaL_checkudata((L), (i), MTNAME_FTS))

#define checkyclfd(L, i) \
  ((struct file_fd *)luaL_checkudata(L, (i), FILE_MTNAME_FD))

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

int file_mkfd(lua_State *L, int fd) {
  struct file_fd *lfd;
  lfd = lua_newuserdata(L, sizeof(struct file_fd));
  lfd->fd = fd;
  luaL_setmetatable(L, FILE_MTNAME_FD);
  return 1;
}

static int l_fdclose(lua_State *L) {
  struct file_fd *lfd;
  lfd = checkyclfd(L, 1);
  if (lfd->fd >= 0) {
    close(lfd->fd);
    lfd->fd = -1;
  }
  return 0;
}

static int l_fdget(lua_State *L) {
  struct file_fd *lfd;
  lfd = checkyclfd(L, 1);
  lua_pushinteger(L, (lua_Integer)lfd->fd);
  return 1;
}

static int l_fdwait(lua_State *L) {
  struct pollfd pfd;
  struct file_fd *lfd;
  int ret;

  lfd = checkyclfd(L, 1);
  pfd.fd = lfd->fd;
  pfd.events = POLLIN | POLLERR;
  do {
    ret = poll(&pfd, 1, INFTIM);
  } while (ret < 0 && errno == EINTR);

  if (ret < 0) {
    return luaL_error(L, "poll: %s", strerror(errno));
  }

  return 0;
}

static int l_streamclose(lua_State *L) {
  luaL_Stream *s;

  s = (luaL_Stream*)luaL_checkudata(L, 1, LUA_FILEHANDLE);
  if (s->f) {
    fclose(s->f);
  }
  s->f = NULL;
  return 0;
}

static int l_fdtostream(lua_State *L) {
  struct file_fd *lfd;
  luaL_Stream *s;
  const char *mode;
  FILE *fp;

  lfd = checkyclfd(L, 1);
  mode = luaL_checkstring(L, 2);

  if (lfd->fd < 0) {
    return luaL_error(L, "to_stream called on closed fd");
  }

  if (strncmp(mode, "zlib:", 5) == 0) {
    fp = zfile_fdopen(lfd->fd, mode + 5);
  } else {
    fp = fdopen(lfd->fd, mode);
  }

  if (fp == NULL) {
    return luaL_error(L, "to_stream: %s",
        (errno == 0) ? "unknown error" : strerror(errno));
  }

  s = lua_newuserdata(L, sizeof(luaL_Stream));
  luaL_setmetatable(L, LUA_FILEHANDLE);
  s->f = fp;
  s->closef = &l_streamclose;
  lfd->fd = -1; /* to avoid double-close, the stream object now owns the fd */
  return 1;
}

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
  {"O_APPEND", O_APPEND},
  {"O_CREAT", O_CREAT},
  {"O_EXCL", O_EXCL},
  {"O_NOCTTY", O_NOCTTY},
  {"O_NONBLOCK", O_NONBLOCK},
  {"O_RDONLY", O_RDONLY},
  {"O_RDWR", O_RDWR},
  {"O_SYNC", O_SYNC},
  {"O_TRUNC", O_TRUNC},
  {"O_WRONLY", O_WRONLY},
  {NULL, 0},
};

static const struct luaL_Reg fd_m[] = {
  {"__gc", l_fdclose},
  {"close", l_fdclose},
  {"get", l_fdget},
  {"wait", l_fdwait},
  {"to_stream", l_fdtostream},
  {NULL, NULL},
};

static const struct luaL_Reg fts_m[] = {
  {"__gc", l_ftsgc},
  {NULL, NULL},
};

static const struct luaL_Reg file_f[] = {
  {"fts", l_fts},
  {NULL, NULL}
};

int luaopen_file(lua_State *L) {
  int i;

  /* create the fts metatable */
  luaL_newmetatable(L, MTNAME_FTS);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, fts_m, 0);

  /* create the fd metatable */
  luaL_newmetatable(L, FILE_MTNAME_FD);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, fd_m, 0);

  /* setup the 'file' library and push constants to it */
  luaL_newlib(L, file_f);
  for (i = 0; g_intconsts[i].name != NULL; i++) {
    lua_pushinteger(L, g_intconsts[i].val);
    lua_setfield(L, -2, g_intconsts[i].name);
  }

  return 1;
}
