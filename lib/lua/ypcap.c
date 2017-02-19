/* vim: set tabstop=2 shiftwidth=2 expandtab ai: */

#include <pcap/pcap.h>

#include <lib/lua/ypcap.h>

#define MTNAME_YPCAP "ypcap.ypcap"

#define checkypcap(L, i) \
    ((ypcap_t*)luaL_checkudata(L, (i), MTNAME_YPCAP))

#define YPCAPF_ISLIVE      (1 << 0)
#define YPCAPF_ISACTIVATED (1 << 1)

typedef struct {
  int flags;
  pcap_t *pcap;
  char errbuf[PCAP_ERRBUF_SIZE];
} ypcap_t;

static int l_ypcap_new(lua_State *L) {
  const char *iface = NULL, *file = NULL;
  ypcap_t *ypcap;

  luaL_checktype(L, 1, LUA_TTABLE);
  ypcap = lua_newuserdata(L, sizeof(ypcap_t));
  luaL_setmetatable(L, MTNAME_YPCAP);
  ypcap->flags = 0;
  ypcap->pcap = NULL;

  lua_getfield(L, 1, "iface");
  if ((iface = lua_tostring(L, -1)) != NULL) {
    if ((ypcap->pcap = pcap_create(iface, ypcap->errbuf)) == NULL) {
      return luaL_error(L, "pcap_create: %s", ypcap->errbuf);
    }
    ypcap->flags |= YPCAPF_ISLIVE;
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "file");
  if (iface == NULL && (file = lua_tostring(L, -1)) != NULL) {
    if ((ypcap->pcap = pcap_open_offline(file, ypcap->errbuf)) == NULL) {
      return luaL_error(L, "pcap_open_offline: %s", ypcap->errbuf);
    }
  }
  lua_pop(L, 1);

  if (iface == NULL && file == NULL) {
    goto fail;
  }

  return 1;
fail:
  luaL_error(L, "neither 'iface' or 'file' set");
  return 0;
}

static int l_ypcap_start(lua_State *L) {
  const char *cptr;
  struct bpf_program bpf;
  int ret;
  ypcap_t *ypcap = checkypcap(L, 1);

  /* activate the pcap handle, if it's a live one */
  if ((ypcap->flags & YPCAPF_ISLIVE) && !(ypcap->flags & YPCAPF_ISACTIVATED)) {
    if (pcap_activate(ypcap->pcap) != 0) {
      return luaL_error(L, pcap_geterr(ypcap->pcap));
    }
    ypcap->flags |= YPCAPF_ISACTIVATED;
  }

  /* set filter, if present. Must be done after pcap_activate, because
   * pcap_activate sets the link layer type of the pcap_t handle which is
   * needed when compiling the filter, at least on libpcap <= 1.7.4  */
  if ((cptr = lua_tostring(L, 2)) != NULL) {
    if (pcap_compile(ypcap->pcap, &bpf, cptr, 1, PCAP_NETMASK_UNKNOWN) < 0) {
      return luaL_error(L, "pcap_compile: %s", pcap_geterr(ypcap->pcap));
    }
    ret = pcap_setfilter(ypcap->pcap, &bpf);
    pcap_freecode(&bpf);
    if (ret < 0) {
      return luaL_error(L, "pcap_setfilter: %s", pcap_geterr(ypcap->pcap));
    }
  }
  lua_pop(L, 1);
  return 0;
}

static int l_ypcap_next(lua_State *L) {
  ypcap_t *ypcap = checkypcap(L, 1);
  int ret;
  struct pcap_pkthdr *pkthdr;
  const u_char *pktdata;

  if ((ypcap->flags & YPCAPF_ISLIVE) && !(ypcap->flags & YPCAPF_ISACTIVATED)) {
    return luaL_error(L, "next called on an inactive live pcap handle");
  }

  ret = pcap_next_ex(ypcap->pcap, &pkthdr, &pktdata);
  if (ret == 1) {
    lua_pushlstring(L, (const char *)pktdata, (size_t)pkthdr->caplen);
    lua_pushinteger(L, (lua_Integer)pkthdr->ts.tv_sec);
    lua_pushinteger(L, (lua_Integer)pkthdr->ts.tv_usec);
    return 3;
  } else if (ret == 0 || ret == -2) {
    /* timeout if live capture, end of file if reading from savefile */
    lua_pushnil(L);
    return 1;
  } else if (ret == -1) {
    return luaL_error(L, "pcap_next_ex: %s", pcap_geterr(ypcap->pcap));
  } else {
    return luaL_error(L, "pcap_next_ex: unexpected return value (%d)", ret);
  }
}

static int l_ypcap_gc(lua_State *L) {
  ypcap_t *ypcap = checkypcap(L, 1);
  if (ypcap->pcap != NULL) {
    pcap_close(ypcap->pcap);
  }
  return 0;
}

static const struct luaL_Reg ypcap_type[] = {
  {"start", l_ypcap_start},
  {"next", l_ypcap_next},
  {"__gc", l_ypcap_gc},
  {NULL, NULL}
};

static const struct luaL_Reg ypcap_lib[] = {
  {"new", l_ypcap_new},
  {NULL, NULL}
};

int luaopen_ypcap(lua_State *L) {
  luaL_newmetatable(L, MTNAME_YPCAP);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, ypcap_type, 0);
  luaL_newlib(L, ypcap_lib);
  return 1;
}
