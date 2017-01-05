/* vim: set tabstop=2 shiftwidth=2 expandtab ai: */
#include <string.h>

#include "3rd_party/lua.h"
#include "3rd_party/libpcap/pcap/pcap.h"
#include "ip.h"

#define MTNAME_ADDR   "yans.Addr"
#define MTNAME_BLOCK  "yans.Block"

#define checkaddr(L, i) \
    ((ip_addr_t*)luaL_checkudata(L, (i), MTNAME_ADDR))

#define checkblock(L, i) \
    ((ip_block_t*)luaL_checkudata(L, (i), MTNAME_BLOCK))


static inline ip_addr_t *l_newipaddr(lua_State *L) {
  ip_addr_t *addr;

  addr = lua_newuserdata(L, sizeof(ip_addr_t));
  luaL_setmetatable(L, MTNAME_ADDR);
  return addr;
}

static inline ip_block_t *l_newblock(lua_State *L) {
  ip_block_t *blk;

  blk = lua_newuserdata(L, sizeof(ip_block_t));
  luaL_setmetatable(L, MTNAME_BLOCK);
  return blk;
}

static inline ip_addr_t *l_pushipaddr(lua_State *L, struct sockaddr *saddr) {
  ip_addr_t *addr;
  addr = l_newipaddr(L);
  if (saddr->sa_family == AF_INET6) {
    memcpy(&addr->u.sin6, saddr, sizeof(addr->u.sin6));
  } else if (saddr->sa_family == AF_INET) {
    memcpy(&addr->u.sin, saddr, sizeof(addr->u.sin));
  } else {
    /* should be checked beforehand */
    luaL_error(L, "unknown address family");
  }
  return addr;
}

static int l_addr(lua_State *L) {
  ip_addr_t *addr;
  const char *s;
  int err = 0;

  s = luaL_checkstring(L, 1);
  addr = l_newipaddr(L);
  if (ip_addr(addr, s, &err) < 0) {
    if (err != 0) {
      luaL_error(L, "Addr: %s, address \"%s\"", ip_addr_strerror(err), s);
    } else {
      luaL_error(L, "Addr: IP address parse error \"%s\"", s);
    }
  }
  return 1;
}

static int l_addr_tostring(lua_State *L) {
  char addrbuf[YANS_IP_ADDR_MAXLEN];
  ip_addr_t *addr;
  int err=0;

  addr = checkaddr(L, 1);
  if (ip_addr_str(addr, addrbuf, sizeof(addrbuf), &err) < 0) {
    if (err != 0) {
      luaL_error(L, "Addr: string conversion error: %s",
          ip_addr_strerror(err));
    } else {
      luaL_error(L, "Addr: string conversion error");
    }
  }
  lua_pushfstring(L, "%s", addrbuf);
  return 1;
}

static int l_addr_eqop(lua_State *L) {
  ip_addr_t *a, *b;
  int err = 0, ret;

  a = checkaddr(L, 1);
  b = checkaddr(L, 2);
  ret = ip_addr_cmp(a, b, &err);
  if (err != 0) {
    luaL_error(L, "Addr: failed comparison: %s\n", ip_addr_strerror(err));
  } else if (ret == 0) {
    lua_pushboolean(L, 1);
  } else {
    lua_pushboolean(L, 0);
  }
  return 1;
}

static int l_addr_ltop(lua_State *L) {
  ip_addr_t *a, *b;
  int err = 0, ret;

  a = checkaddr(L, 1);
  b = checkaddr(L, 2);
  ret = ip_addr_cmp(a, b, &err);
  if (err != 0) {
    luaL_error(L, "Addr: failed comparison: %s\n", ip_addr_strerror(err));
  } else if (ret < 0) {
    lua_pushboolean(L, 1);
  } else {
    lua_pushboolean(L, 0);
  }
  return 1;
}

static int l_addr_leop(lua_State *L) {
  ip_addr_t *a, *b;
  int err = 0, ret;

  a = checkaddr(L, 1);
  b = checkaddr(L, 2);
  ret = ip_addr_cmp(a, b, &err);
  if (err != 0) {
    luaL_error(L, "Addr: failed comparison: %s\n", ip_addr_strerror(err));
  } else if (ret <= 0) {
    lua_pushboolean(L, 1);
  } else {
    lua_pushboolean(L, 0);
  }
  return 1;
}

/* ip_addr arithmetic depends on signed 32-bit integers */
static void l_addr_checkipnum(lua_State *L, lua_Integer n) {
  if (sizeof(lua_Integer) > sizeof(uint32_t)) {
    if (n < -2147483648 || n > 2147483647) {
      luaL_error(L, "Addr: added number is out of range");
    }
  }
}

/* get the ip_addr_t and the */
static void l_addr_arithops(lua_State *L, ip_addr_t **addr, lua_Integer *n) {
  int isnum;

  if ((*addr = luaL_testudata(L, 1, MTNAME_ADDR)) == NULL &&
      (*addr = luaL_testudata(L, 2, MTNAME_ADDR)) == NULL) {
    luaL_error(L, "non-"MTNAME_ADDR" 'add' operation");
  }

  *n = lua_tointegerx(L, 2, &isnum);
  if (!isnum) {
    *n = lua_tointegerx(L, 1, &isnum);
    if (!isnum) {
      luaL_error(L, "attempted to add non-integer value to "MTNAME_ADDR);
    }
  }

  l_addr_checkipnum(L, *n);
}

static int l_addr_addop(lua_State *L) {
  ip_addr_t *addr, *newaddr;
  lua_Integer n;

  l_addr_arithops(L, &addr, &n);
  newaddr = l_newipaddr(L);
  memcpy(newaddr, addr, sizeof(ip_addr_t));
  ip_addr_add(newaddr, (int32_t)n);
  return 1;
}

static int l_addr_subop(lua_State *L) {
  ip_addr_t *addr, *newaddr;
  lua_Integer n;

  l_addr_arithops(L, &addr, &n);
  newaddr = l_newipaddr(L);
  memcpy(newaddr, addr, sizeof(ip_addr_t));
  ip_addr_sub(newaddr, (int32_t)n);
  return 1;
}

static int l_addr_add(lua_State *L) {
  ip_addr_t *addr;
  lua_Integer n;

  addr = checkaddr(L, 1);
  n = luaL_checkinteger(L, 2);
  l_addr_checkipnum(L, n);
  ip_addr_add(addr, (int32_t)n);
  lua_pushvalue(L, 1);
  return 1;
}

static int l_addr_sub(lua_State *L) {
  ip_addr_t *addr;
  lua_Integer n;

  addr = checkaddr(L, 1);
  n = luaL_checkinteger(L, 2);
  l_addr_checkipnum(L, n);
  ip_addr_sub(addr, (int32_t)n);
  lua_pushvalue(L, 1);
  return 1;
}

static int l_block(lua_State *L) {
  ip_block_t *blk;
  const char *s;
  int err = 0;

  s = luaL_checkstring(L, 1);
  blk = l_newblock(L);
  if (ip_block(blk, s, &err) < 0) {
    if (err != 0) {
      luaL_error(L, "Block: %s, block \"%s\"", ip_block_strerror(err), s);
    } else {
      luaL_error(L, "Block: parse error \"%s\"", s);
    }
  }
  return 1;
}

static int l_block_tostring(lua_State *L) {
  char addrbuf[YANS_IP_ADDR_MAXLEN*2+2];
  ip_block_t *addr;
  int err=0;

  addr = checkblock(L, 1);
  if (ip_block_str(addr, addrbuf, sizeof(addrbuf), &err) < 0) {
    if (err != 0) {
      luaL_error(L, "Addr: string conversion error: %s",
          ip_addr_strerror(err));
    } else {
      luaL_error(L, "Addr: string conversion error");
    }
  }
  lua_pushfstring(L, "%s", addrbuf);
  return 1;
}

static int l_block_first(lua_State *L) {
  ip_block_t *blk;
  ip_addr_t *addr;

  blk = checkblock(L, 1);
  addr = l_newipaddr(L);
  memcpy(addr, &blk->first, sizeof(ip_addr_t));
  return 1;
}

static int l_block_last(lua_State *L) {
  ip_block_t *blk;
  ip_addr_t *addr;

  blk = checkblock(L, 1);
  addr = l_newipaddr(L);
  memcpy(addr, &blk->last, sizeof(ip_addr_t));
  return 1;
}

static int l_block_contains(lua_State *L) {
  ip_block_t *blk;
  ip_addr_t *addr;
  int ret;

  blk = checkblock(L, 1);
  addr = checkaddr(L, 2);
  if ((ret = ip_block_contains(blk, addr, NULL)) < 0) {
    luaL_error(L, "contains: incompatible address types");
  }

  lua_pushboolean(L, ret);
  return 1;
}

int ip_block_contains(ip_block_t *blk, ip_addr_t *addr, int *err);

/* returns the number of device addresses, and if > 0 a table on TOS */
static lua_Integer l_device_addrs(lua_State*L, pcap_addr_t *addrs) {
  pcap_addr_t *curr;
  lua_Integer naddrs = 0;
  ip_addr_t *addr = NULL, *netmask = NULL;
  ip_block_t *blk;

  for (curr = addrs; curr != NULL; curr = curr->next) {
    if (curr->addr == NULL
        || (curr->addr->sa_family != AF_INET &&
            curr->addr->sa_family != AF_INET6)) {
      continue;
    }
    if (naddrs == 0) {
      lua_newtable(L);
    }

    lua_newtable(L);
    addr = l_pushipaddr(L, curr->addr);
    lua_setfield(L, -2, "addr");
    naddrs++;

    if (curr->netmask != NULL) {
      netmask = l_pushipaddr(L, curr->netmask);
      lua_setfield(L, -2, "netmask");
    }

    if (curr->broadaddr != NULL) {
      l_pushipaddr(L, curr->broadaddr);
      lua_setfield(L, -2, "broadaddr");
    }

    if (curr->dstaddr != NULL) {
      l_pushipaddr(L, curr->dstaddr);
      lua_setfield(L, -2, "dstaddr");
    }

    if (addr != NULL && netmask != NULL) {
      blk = l_newblock(L);
      if (ip_block_netmask(blk, addr, netmask, NULL) == 0) {
        lua_setfield(L, -2, "block");
      } else {
        lua_pop(L, 1);
      }
    }

    lua_rawseti(L, -2, naddrs);
  }

  return naddrs;
}

/* returns LUA_OK with table on TOS, or LUA_ERRERR with
 * error on TOS */
static void l_device(lua_State *L, pcap_if_t *dev) {

  lua_newtable(L);
  if (dev->name != NULL) {
    lua_pushstring(L, dev->name);
    lua_setfield(L, -2, "name");
  }

  if (dev->description != NULL) {
    lua_pushstring(L, dev->description);
    lua_setfield(L, -2, "description");
  }

  if (dev->flags & PCAP_IF_LOOPBACK) {
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "loopback");
  }

  if (dev->flags & PCAP_IF_UP) {
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "up");
  }

  if (dev->flags & PCAP_IF_RUNNING) {
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "running");
  }

  if (l_device_addrs(L, dev->addresses) > 0) {
    lua_setfield(L, -2, "addresses");
  }
}

static int l_devices(lua_State *L) {
  pcap_if_t *devs, *curr;
  char errbuf[PCAP_ERRBUF_SIZE];
  lua_Integer ndevs=0;

  if (pcap_findalldevs(&devs, errbuf) < 0) {
    return luaL_error(L, "%s", errbuf);
  }

  lua_newtable(L);
  for (curr = devs; curr != NULL; curr = curr->next) {
    l_device(L, curr);
    ndevs++;
    lua_rawseti(L, -2, ndevs);
  }

  pcap_freealldevs(devs);
  return 1;
}

static const struct luaL_Reg yansaddr_m[] = {
  {"__tostring", l_addr_tostring},
  {"__eq", l_addr_eqop},
  {"__lt", l_addr_ltop},
  {"__le", l_addr_leop},

  /* immutable */
  {"__add", l_addr_addop},
  {"__sub", l_addr_subop},

  /* mutable */
  {"add", l_addr_add},
  {"sub", l_addr_sub},
  {NULL, NULL}
};

static const struct luaL_Reg yansblock_m[] = {
  /* TODO:
  {"__len", NULL}, how to handle too large length? Error? LUA_MAXINTEGER? Multiple integers (incompatible with __len)?
  {"__div", NULL},  split blocks into sub-blocks
  */
  {"__tostring", l_block_tostring},
  {"first", l_block_first},
  {"last", l_block_last},
  {"contains", l_block_contains},
  {NULL, NULL}
};

static const struct luaL_Reg yans_f[] = {
	{"Addr", l_addr},
  {"Block", l_block},
  {"devices", l_devices},
	{NULL, NULL}
};

int luaopen_yans(lua_State *L) {
  /* create the metatable for Addr and register its methods */
  luaL_newmetatable(L, MTNAME_ADDR);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, yansaddr_m, 0);

  /* create the metatable for Block and register its methods */
  luaL_newmetatable(L, MTNAME_BLOCK);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, yansblock_m, 0);

  luaL_newlib(L, yans_f);
  return 1;
}
