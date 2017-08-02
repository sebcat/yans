#include <string.h>
#include <errno.h>

#include <lib/util/io.h>
#include <lib/util/fio.h>
#include <lib/lua/pcapd.h>

#include <proto/pcap_req.h>
#include <proto/status_resp.h>

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

  if ((pcapd->fio = fdopen(IO_FILENO(&sock), "w")) == NULL) {
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
  struct p_pcap_req cmd;
  struct p_status_resp resp;
  struct pcapd *pcapd = checkpcapd(L, 1);
  const char *dumppath = luaL_checkstring(L, 2);
  size_t nread;
  buf_t buf;
  char errbuf[256];

  cmd.iface = luaL_checklstring(L, 3, &cmd.ifacelen);
  cmd.filter = lua_tolstring(L, 4, &cmd.filterlen);

  if (pcapd->fio == NULL) {
    return luaL_error(L, "attempted to open a disconnected session");
  }

  if (io_open(&dumpfile, dumppath, O_WRONLY|O_CREAT|O_TRUNC, 0660) != IO_OK) {
    return luaL_error(L, "dumpfile: %s", io_strerror(&dumpfile));
  }

  IO_INIT(&cli, fileno(pcapd->fio));
  ret = io_sendfd(&cli, IO_FILENO(&dumpfile));
  io_close(&dumpfile);
  if (ret != IO_OK) {
    return luaL_error(L, "sendfd: %s", io_strerror(&cli));
  }

  buf_init(&buf, 1024);
  if (p_pcap_req_serialize(&cmd, &buf) != PROTO_OK) {
    buf_cleanup(&buf);
    return luaL_error(L, "unable to serialize pcap cmd");
  }

  ret = io_writeall(&cli, buf.data, buf.len);
  if (ret != IO_OK) {
    buf_cleanup(&buf);
    return luaL_error(L, "unable to send command: %s", io_strerror(&cli));
  }

  /* read response message */
  buf_clear(&buf);
  do {
    ret = io_readbuf(&cli, &buf, &nread);
    if (ret != IO_OK) {
      buf_cleanup(&buf);
      return luaL_error(L, "error reading response: %s", io_strerror(&cli));
    } else if (nread == 0) {
      return luaL_error(L, "connection closed while reading response");
    }
  } while ((ret = p_status_resp_deserialize(&resp, buf.data, buf.len, NULL)) ==
        PROTO_ERRINCOMPLETE);
  if (resp.errmsg != NULL) {
    /* copy resp.errmsg to the stack, since it's a part of buf, which we
     * want to cleanup before luaL_error */
    snprintf(errbuf, sizeof(errbuf), "%s", resp.errmsg);
    buf_cleanup(&buf);
    luaL_error(L, "%s", errbuf);
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

