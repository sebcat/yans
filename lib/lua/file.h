#ifndef YLUA_FILE_H__
#define YLUA_FILE_H__

#include <3rd_party/lua.h>

#define FILE_MTNAME_FD  "file.FD"
struct file_fd {
  int fd;
};

int file_mkfd(lua_State *L, int fd);

int luaopen_file(lua_State *L);

#endif
