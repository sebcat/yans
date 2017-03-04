#include <string.h>
#include <errno.h>

#include <lib/util/io.h>
#include <lib/util/fio.h>
#include <lib/lua/pcapd.h>

#define MTNAME_PCAPD "pcapd.pcapd"

#define checkpcapd(L, i) \
    ((struct pcapd *)luaL_checkudata(L, (i), MTNAME_PCAPD))

struct pcapd {
  FILE *fio;
};

/* create a new pcapd_t handle and connect to pcapd */
static int l_pcapd_connect(lua_State *L) {
  struct pcapd *pcapd;
  io_t sock;
  const char *sockpath = luaL_checkstring(L, 1);

  pcapd = lua_newuserdata(L, sizeof(struct pcapd));
  pcapd->fio = NULL;
  luaL_setmetatable(L, MTNAME_PCAPD);
  if (io_connect_unix(&sock, sockpath) != IO_OK) {
    return luaL_error(L, "%s", io_strerror(&sock));
  }

  if ((pcapd->fio = fdopen(io_fileno(&sock), "w")) == NULL) {
    io_close(&sock);
    return luaL_error(L, "fdopen: %s", strerror(errno));
  }
  return 1;
}

/* clean up internal state of struct pcapd. Must be possible to call multiple
 * times since it's called both on graceful close and GC. */
static int l_pcapd_gc(lua_State *L) {
  struct pcapd *pcapd = checkpcapd(L, 1);
  if (pcapd->fio != NULL) {
    fclose(pcapd->fio);
    pcapd->fio = NULL;
  }
  return 0;
}

/* open a new dump */
static int l_pcapd_open(lua_State *L) {
  io_t cli;
  io_t dumpfile;
  int ret;
  size_t ifacelen;
  size_t filterlen;
  struct pcapd *pcapd = checkpcapd(L, 1);
  const char *dumppath = luaL_checkstring(L, 2);
  const char *iface = luaL_checklstring(L, 3, &ifacelen);
  const char *filter = lua_tolstring(L, 4, &filterlen);

  if (filter == NULL) {
    filter = "";
    filterlen = 0;
  }

  if (io_open(&dumpfile, dumppath, O_WRONLY|O_CREAT|O_TRUNC, 0660) != IO_OK) {
    return luaL_error(L, "dumpfile: %s", io_strerror(&dumpfile));
  }

  io_init(&cli, fileno(pcapd->fio));
  ret = io_sendfd(&cli, io_fileno(&dumpfile));
  io_close(&dumpfile);
  if (ret != IO_OK) {
    return luaL_error(L, "sendfd: %s", io_strerror(&cli));
  }

  if ((ret = fio_writens(pcapd->fio, iface, ifacelen)) != FIO_OK) {
    return luaL_error(L, "%s: %s", iface, fio_strerror(ret));
  }

  if ((ret = fio_writens(pcapd->fio, filter, filterlen)) != FIO_OK) {
    return luaL_error(L, "filter: %s", fio_strerror(ret));
  }

  return 0;
}

/* close the dump, and call l_pcapd_gc to clean up internal state */
static int l_pcapd_close(lua_State *L) {
  char buf[16];
  struct pcapd *pcapd = checkpcapd(L, 1);
  if (pcapd->fio != NULL) {
    /* graceful close */
    fio_writens(pcapd->fio, "", 0);
    fio_readns(pcapd->fio, buf, sizeof(buf));
  }
  return l_pcapd_gc(L);
}

static const struct luaL_Reg pcapd_type[] = {
  {"open", l_pcapd_open},
  {"close", l_pcapd_close},
  {"__gc", l_pcapd_gc},
  {NULL, NULL}
};

static const struct luaL_Reg pcapd_lib[] = {
  {"connect", l_pcapd_connect},
  {NULL, NULL}
};

int luaopen_pcapd(lua_State *L) {
  luaL_newmetatable(L, MTNAME_PCAPD);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, pcapd_type, 0);
  luaL_newlib(L, pcapd_lib);
  return 1;
}

