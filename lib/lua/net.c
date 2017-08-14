/* vim: set tabstop=2 shiftwidth=2 expandtab ai: */
#include <string.h>

#include <pcap/pcap.h>

#include <lib/net/ip.h>
#include <lib/net/ports.h>
#include <lib/net/eth.h>
#include <lib/net/route.h>
#include <lib/net/iface.h>
#include <lib/lua/net.h>

#define MTNAME_IPADDR     "yans.IPAddr"
#define MTNAME_IPPORTS    "yans.IPPorts"
#define MTNAME_ETHADDR    "yans.EthAddr"
#define MTNAME_BLOCK      "yans.IPBlock"
#define MTNAME_BLOCKS     "yans.IPBlocks"

#define checkipaddr(L, i) \
    ((ip_addr_t*)luaL_checkudata(L, (i), MTNAME_IPADDR))

#define checkipports(L, i) \
    ((struct port_ranges *)luaL_checkudata(L, (i), MTNAME_IPPORTS))

#define checkethaddr(L, i) \
    ((struct eth_addr *)luaL_checkudata(L, (i), MTNAME_ETHADDR))

#define checkblock(L, i) \
    ((ip_block_t*)luaL_checkudata(L, (i), MTNAME_BLOCK))

#define checkblocks(L, i) \
    ((struct ip_blocks *)luaL_checkudata(L, (i), MTNAME_BLOCKS))

static inline ip_addr_t *l_newipaddr(lua_State *L) {
  ip_addr_t *addr;

  addr = lua_newuserdata(L, sizeof(ip_addr_t));
  luaL_setmetatable(L, MTNAME_IPADDR);
  return addr;
}

static inline struct port_ranges *l_newipports(lua_State *L) {
  struct port_ranges *rs;

  rs = lua_newuserdata(L, sizeof(struct port_ranges));
  luaL_setmetatable(L, MTNAME_IPPORTS);
  return rs;
}

static inline struct eth_addr *l_newethaddr(lua_State *L) {
  struct eth_addr *eth;

  eth = lua_newuserdata(L, sizeof(struct eth_addr));
  luaL_setmetatable(L, MTNAME_ETHADDR);
  return eth;
}

static inline ip_block_t *l_newipblock(lua_State *L) {
  ip_block_t *blk;

  blk = lua_newuserdata(L, sizeof(ip_block_t));
  luaL_setmetatable(L, MTNAME_BLOCK);
  return blk;
}

static inline struct ip_blocks *l_newipblocks(lua_State *L) {
  struct ip_blocks *blks;

  blks = lua_newuserdata(L, sizeof(struct ip_blocks));
  luaL_setmetatable(L, MTNAME_BLOCKS);
  return blks;
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

static int l_ipaddr(lua_State *L) {
  ip_addr_t *addr;
  const char *s;
  int err = 0;

  s = luaL_checkstring(L, 1);
  addr = l_newipaddr(L);
  if (ip_addr(addr, s, &err) < 0) {
    if (err != 0) {
      luaL_error(L, "IPAddr: %s, address \"%s\"", ip_addr_strerror(err), s);
    } else {
      luaL_error(L, "IPAddr: IP address parse error \"%s\"", s);
    }
  }
  return 1;
}

static int l_ipaddr_tostring(lua_State *L) {
  char addrbuf[YANS_IP_ADDR_MAXLEN];
  ip_addr_t *addr;
  int err=0;

  addr = checkipaddr(L, 1);
  if (ip_addr_str(addr, addrbuf, sizeof(addrbuf), &err) < 0) {
    if (err != 0) {
      luaL_error(L, "IPAddr: string conversion error: %s",
          ip_addr_strerror(err));
    } else {
      luaL_error(L, "IPAddr: string conversion error");
    }
  }
  lua_pushfstring(L, "%s", addrbuf);
  return 1;
}

static int l_ipaddr_eqop(lua_State *L) {
  ip_addr_t *a, *b;
  int err = 0, ret;

  a = checkipaddr(L, 1);
  b = checkipaddr(L, 2);
  ret = ip_addr_cmp(a, b, &err);
  if (err != 0) {
    luaL_error(L, "IPAddr: failed comparison: %s\n", ip_addr_strerror(err));
  } else if (ret == 0) {
    lua_pushboolean(L, 1);
  } else {
    lua_pushboolean(L, 0);
  }
  return 1;
}

static int l_ipaddr_ltop(lua_State *L) {
  ip_addr_t *a, *b;
  int err = 0, ret;

  a = checkipaddr(L, 1);
  b = checkipaddr(L, 2);
  ret = ip_addr_cmp(a, b, &err);
  if (err != 0) {
    luaL_error(L, "IPAddr: failed comparison: %s\n", ip_addr_strerror(err));
  } else if (ret < 0) {
    lua_pushboolean(L, 1);
  } else {
    lua_pushboolean(L, 0);
  }
  return 1;
}

static int l_ipaddr_leop(lua_State *L) {
  ip_addr_t *a, *b;
  int err = 0, ret;

  a = checkipaddr(L, 1);
  b = checkipaddr(L, 2);
  ret = ip_addr_cmp(a, b, &err);
  if (err != 0) {
    luaL_error(L, "IPAddr: failed comparison: %s\n", ip_addr_strerror(err));
  } else if (ret <= 0) {
    lua_pushboolean(L, 1);
  } else {
    lua_pushboolean(L, 0);
  }
  return 1;
}

/* ip_addr arithmetic depends on signed 32-bit integers */
static void l_ipaddr_checkipnum(lua_State *L, lua_Integer n) {
  if (sizeof(lua_Integer) > sizeof(uint32_t)) {
    if (n < -2147483648 || n > 2147483647) {
      luaL_error(L, "IPAddr: added number is out of range");
    }
  }
}

/* get the ip_addr_t and the */
static void l_ipaddr_arithops(lua_State *L, ip_addr_t **addr, lua_Integer *n) {
  int isnum;

  if ((*addr = luaL_testudata(L, 1, MTNAME_IPADDR)) == NULL &&
      (*addr = luaL_testudata(L, 2, MTNAME_IPADDR)) == NULL) {
    luaL_error(L, "non-"MTNAME_IPADDR" 'add' operation");
  }

  *n = lua_tointegerx(L, 2, &isnum);
  if (!isnum) {
    *n = lua_tointegerx(L, 1, &isnum);
    if (!isnum) {
      luaL_error(L, "attempted to add non-integer value to "MTNAME_IPADDR);
    }
  }

  l_ipaddr_checkipnum(L, *n);
}

static int l_ipaddr_addop(lua_State *L) {
  ip_addr_t *addr, *newaddr;
  lua_Integer n;

  l_ipaddr_arithops(L, &addr, &n);
  newaddr = l_newipaddr(L);
  memcpy(newaddr, addr, sizeof(ip_addr_t));
  ip_addr_add(newaddr, (int32_t)n);
  return 1;
}

static int l_ipaddr_subop(lua_State *L) {
  ip_addr_t *addr, *newaddr;
  lua_Integer n;

  l_ipaddr_arithops(L, &addr, &n);
  newaddr = l_newipaddr(L);
  memcpy(newaddr, addr, sizeof(ip_addr_t));
  ip_addr_sub(newaddr, (int32_t)n);
  return 1;
}

static int l_ipaddr_add(lua_State *L) {
  ip_addr_t *addr;
  lua_Integer n;

  addr = checkipaddr(L, 1);
  n = luaL_checkinteger(L, 2);
  l_ipaddr_checkipnum(L, n);
  ip_addr_add(addr, (int32_t)n);
  lua_pushvalue(L, 1);
  return 1;
}

static int l_ipaddr_sub(lua_State *L) {
  ip_addr_t *addr;
  lua_Integer n;

  addr = checkipaddr(L, 1);
  n = luaL_checkinteger(L, 2);
  l_ipaddr_checkipnum(L, n);
  ip_addr_sub(addr, (int32_t)n);
  lua_pushvalue(L, 1);
  return 1;
}

static int l_ethaddr(lua_State *L) {
  struct eth_addr *eth;
  size_t len;
  const char *addr;
  char ethbuf[ETH_ALEN];
  int ret;

  addr = luaL_checklstring(L, 1, &len);
  if (len < 6) {
    return luaL_error(L, "EthAddr: ethernet address too short");
  } else if (len == 6) {
    /* raw byte format */
    eth = l_newethaddr(L);
    eth_addr_init_bytes(eth, addr);
  } else if (len > 6) {
    /* assume XX:XX:XX:XX:XX:XX */
    ret = sscanf(addr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &ethbuf[0],
        &ethbuf[1], &ethbuf[2], &ethbuf[3], &ethbuf[4], &ethbuf[5]);
    if (ret != 6) {
      return luaL_error(L, "EthAddr: invalid ethernet address");
    }
    eth = l_newethaddr(L);
    eth_addr_init_bytes(eth, ethbuf);
  }
  return 1;
}


static int l_ethaddr_tostring(lua_State *L) {
  char addr[ETH_STRSZ];
  struct eth_addr *eth = checkethaddr(L, 1);
  eth_addr_tostring(eth, addr, ETH_STRSZ);
  lua_pushstring(L, addr);
  return 1;
}

static int l_ethaddr_ifindex(lua_State *L) {
  struct eth_addr *eth = checkethaddr(L, 1);
  lua_pushinteger(L, (lua_Integer)eth->index);
  return 1;
}

static int l_ethaddr_bytes(lua_State *L) {
  struct eth_addr *eth = checkethaddr(L, 1);
  lua_pushlstring(L, (const char *)eth->addr, ETH_ALEN);
  return 1;
}

static int l_ipblock(lua_State *L) {
  ip_block_t *blk;
  const char *s;
  int err = 0;

  s = luaL_checkstring(L, 1);
  blk = l_newipblock(L);
  if (ip_block_from_str(blk, s, &err) < 0) {
    if (err != 0) {
      luaL_error(L, "IPBlock: %s, block \"%s\"", ip_block_strerror(err), s);
    } else {
      luaL_error(L, "IPBlock: parse error \"%s\"", s);
    }
  }
  return 1;
}

static int l_ipblock_tostring(lua_State *L) {
  char addrbuf[YANS_IP_ADDR_MAXLEN*2+2];
  ip_block_t *addr;
  int err=0;

  addr = checkblock(L, 1);
  if (ip_block_to_str(addr, addrbuf, sizeof(addrbuf), &err) < 0) {
    if (err != 0) {
      luaL_error(L, "IPAddr: string conversion error: %s",
          ip_addr_strerror(err));
    } else {
      luaL_error(L, "IPAddr: string conversion error");
    }
  }
  lua_pushfstring(L, "%s", addrbuf);
  return 1;
}

static int l_ipblock_first(lua_State *L) {
  ip_block_t *blk;
  ip_addr_t *addr;

  blk = checkblock(L, 1);
  addr = l_newipaddr(L);
  memcpy(addr, &blk->first, sizeof(ip_addr_t));
  return 1;
}

static int l_ipblock_last(lua_State *L) {
  ip_block_t *blk;
  ip_addr_t *addr;

  blk = checkblock(L, 1);
  addr = l_newipaddr(L);
  memcpy(addr, &blk->last, sizeof(ip_addr_t));
  return 1;
}

static int l_ipblock_contains(lua_State *L) {
  ip_block_t *blk;
  ip_addr_t *addr;
  ip_addr_t a1;
  const char *s;
  int ret;

  blk = checkblock(L, 1);
  if (lua_type(L, 2) == LUA_TSTRING) {
    s = luaL_checklstring(L, 2, NULL);
    if (ip_addr(&a1, s, NULL) < 0) {
      return luaL_error(L, "unable to parse address \"%s\"", s);
    }
    addr = &a1;
  } else {
    addr = checkipaddr(L, 2);
  }
  ret = ip_block_contains(blk, addr);
  lua_pushboolean(L, ret);
  return 1;
}

static int l_ipblocks(lua_State *L) {
  struct ip_blocks *blks;
  const char *s;
  int err = 0;

  s = luaL_checkstring(L, 1);
  blks = l_newipblocks(L);
  if (ip_blocks_init(blks, s, &err) < 0) {
    if (err != 0) {
      luaL_error(L, "IPBlock: %s, block \"%s\"", ip_block_strerror(err), s);
    } else {
      luaL_error(L, "IPBlock: parse error \"%s\"", s);
    }
  }
  return 1;
}

static int l_ipblocks_gc(lua_State *L) {
  struct ip_blocks *blks = checkblocks(L, 1);
  ip_blocks_cleanup(blks);
  return 0;
}

static int l_ipblocks_tostring(lua_State *L) {
  buf_t buf;
  int err = 0;
  struct ip_blocks *blks = checkblocks(L, 1);
  buf_init(&buf, 1024);
  if (ip_blocks_to_buf(blks, &buf, &err) < 0) {
    buf_cleanup(&buf);
    return luaL_error(L, "IPBlocks: %s", ip_blocks_strerror(err));
  }
  lua_pushlstring(L, buf.data, buf.len);
  buf_cleanup(&buf);
  return 1;
}

static int l_ipblocks_next_iter(lua_State *L) {
  struct ip_blocks *blks = checkblocks(L, lua_upvalueindex(1));
  ip_addr_t *addr;

  addr = l_newipaddr(L);
  return ip_blocks_next(blks, addr);
}

static int l_ipblocks_next(lua_State *L) {
  checkblocks(L, 1);
  lua_pushcclosure(L, l_ipblocks_next_iter, 1);
  return 1;
}

static int l_ipblocks_contains(lua_State *L) {
  struct ip_blocks *blks;
  ip_addr_t *addr;
  ip_addr_t a1;
  const char *s;
  int ret;

  blks = checkblocks(L, 1);
  if (lua_type(L, 2) == LUA_TSTRING) {
    s = luaL_checklstring(L, 2, NULL);
    if (ip_addr(&a1, s, NULL) < 0) {
      return luaL_error(L, "unable to parse address \"%s\"", s);
    }
    addr = &a1;
  } else {
    addr = checkipaddr(L, 2);
  }

  ret = ip_blocks_contains(blks, addr);
  lua_pushboolean(L, ret);
  return 1;
}

static int l_addroute(lua_State *L, struct route_table_entry *ent) {
  ip_block_t *blk;
  int ret;

  blk = l_newipblock(L);

  if (ent->flags & RTENTRY_HOST) {
    /* it's a host entry */
    ret = ip_block_from_addrs(blk, (ip_addr_t*)&ent->addr.sa,
        (ip_addr_t*)&ent->addr.sa, NULL);
    if (ret < 0) {
      lua_pop(L, 1);
      return -1;
    } else {
      lua_setfield(L, -2, "block");
    }
  } else if (ip_block_netmask(blk, (ip_addr_t*)&ent->addr.sa,
      (ip_addr_t*)&ent->mask.sa, NULL) == 0) {
    /* it's a netmask entry */
    lua_setfield(L, -2, "block");
  } else {
    lua_pop(L, 1);
    return -1;
  }

  if (ent->gw_ifindex >= 0) {
    lua_pushinteger(L, (lua_Integer)ent->gw_ifindex);
    lua_setfield(L, -2, "gw_ifindex");
  } else {
    l_pushipaddr(L, &ent->gw.sa);
    lua_setfield(L, -2, "gw");
  }

  lua_pushinteger(L, (lua_Integer)ent->ifindex);
  lua_setfield(L, -2, "ifindex");

  return 0;
}

static int l_routes(lua_State *L) {
  struct route_table rt;
  char errbuf[ROUTE_TABLE_ERRBUFSZ];
  struct route_table_entry ent;
  lua_Integer i;

  if (route_table_init(&rt) < 0) {
    route_table_strerror(&rt, errbuf, sizeof(errbuf));
    return luaL_error(L, "route_table_init: %s", errbuf);
  }

  lua_createtable(L, 0, 2); /* top-level table containing ip4, ip6 keys */

  lua_newtable(L); /* sub-level table containing a sequence of entries */
  i = 1;
  while (route_table_next_ip4(&rt, &ent)) {
    lua_newtable(L); /* a single routing table entry */
    if (l_addroute(L, &ent) < 0) {
      lua_pop(L, 1);
      continue;
    }
    lua_rawseti(L, -2, i++);
  }
  lua_setfield(L, -2, "ip4");

  lua_newtable(L);
  i = 1;
  while (route_table_next_ip6(&rt, &ent)) {
    lua_newtable(L); /* a single routing table entry */
    if (l_addroute(L, &ent) < 0) {
      lua_pop(L, 1);
      continue;
    }
    lua_rawseti(L, -2, i++);
  }
  lua_setfield(L, -2, "ip6");
  route_table_cleanup(&rt);
  return 1;
}

static int l_ifaces(lua_State *L) {
  struct iface ifs;
  struct iface_entry ent;
  struct eth_addr *eth;
  lua_Integer i;

  if (iface_init(&ifs) < 0) {
    return luaL_error(L, "%s", iface_strerror(&ifs));
  }

  lua_newtable(L);
  i = 1;
  while (iface_next(&ifs, &ent) > 0) {
    lua_createtable(L, 0, 3);
    eth = l_newethaddr(L);
    eth_addr_init_bytes(eth, ent.addr);
    lua_setfield(L, -2, "addr");
    lua_pushinteger(L, (lua_Integer)ent.index);
    lua_setfield(L, -2, "index");
    lua_pushstring(L, ent.name);
    lua_setfield(L, -2, "name");
    lua_pushboolean(L, ent.flags & IFACE_UP);
    lua_setfield(L, -2, "up");
    lua_pushboolean(L, ent.flags & IFACE_LOOPBACK);
    lua_setfield(L, -2, "loopback");
    lua_rawseti(L, -2, i++);
  }

  iface_cleanup(&ifs);
  return 1;
}

static int l_ipports(lua_State *L) {
  const char *s;
  int ret;
  struct port_ranges *rs;
  size_t fail_off;

  s = luaL_checkstring(L, 1);
  rs = l_newipports(L);
  ret = port_ranges_from_str(rs, s, &fail_off);
  if (ret < 0) {
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_pushinteger(L, (lua_Integer)fail_off + 1); /* 1-indexing */
    return 2;
  }

  return 1;
}

static int l_ipports_tostring(lua_State *L) {
  buf_t buf;
  struct port_ranges *rs = checkipports(L, 1);

  if (buf_init(&buf, 1024) == NULL) {
    goto fail;
  }

  if (port_ranges_to_buf(rs, &buf) < 0) {
    buf_cleanup(&buf);
    goto fail;
  }

  lua_pushlstring(L, buf.data, buf.len);
  buf_cleanup(&buf);
  return 1;

fail:
  return luaL_error(L, "memory allocation error");
}

static int l_ipports_gc(lua_State *L) {
  struct port_ranges *rs = checkipports(L, 1);
  port_ranges_cleanup(rs);
  return 0;
}

static const struct luaL_Reg yansipaddr_m[] = {
  {"__tostring", l_ipaddr_tostring},
  {"__eq", l_ipaddr_eqop},
  {"__lt", l_ipaddr_ltop},
  {"__le", l_ipaddr_leop},

  /* immutable */
  {"__add", l_ipaddr_addop},
  {"__sub", l_ipaddr_subop},

  /* mutable */
  {"add", l_ipaddr_add},
  {"sub", l_ipaddr_sub},
  {NULL, NULL}
};

static const struct luaL_Reg yansipports_m[] = {
  {"__tostring", l_ipports_tostring},
  {"__gc", l_ipports_gc},
  {NULL, NULL},
};

static const struct luaL_Reg yansethaddr_m[] = {
  {"__tostring", l_ethaddr_tostring},
  {"ifindex", l_ethaddr_ifindex},
  {"bytes", l_ethaddr_bytes},
  {NULL, NULL}
};

static const struct luaL_Reg yansblock_m[] = {
  /* TODO:
     {"__len", NULL}, how to handle too large length? Error? LUA_MAXINTEGER?
     Multiple integers (incompatible with __len)?
     {"__div", NULL},  split blocks into sub-blocks
   */
  {"__tostring", l_ipblock_tostring},
  {"first", l_ipblock_first},
  {"last", l_ipblock_last},
  {"contains", l_ipblock_contains},
  {NULL, NULL}
};

static const struct luaL_Reg yansblocks_m[] = {
  {"__tostring", l_ipblocks_tostring},
  {"__gc", l_ipblocks_gc},
  {"next", l_ipblocks_next},
  {"contains", l_ipblocks_contains},
  {NULL, NULL},
};

static const struct luaL_Reg ip_f[] = {
  {"addr", l_ipaddr},
  {"addrs", l_ipblocks},
  {"block", l_ipblock},
  {"ports", l_ipports},
  {"routes", l_routes},
  {NULL, NULL},
};

static const struct luaL_Reg eth_f[] = {
  {"addr", l_ethaddr},
  {"ifaces", l_ifaces},
  {NULL, NULL},
};

struct mtable {
  const char *mt;
  const struct luaL_Reg *reg;
};

static void init_mtable(lua_State *L, struct mtable *tbl) {
  size_t i;

  for(i=0; tbl[i].mt != NULL; i++) {
    luaL_newmetatable(L, tbl[i].mt);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, tbl[i].reg, 0);
  }
}

int luaopen_ip(lua_State *L) {
  struct mtable mt[] = {
    {MTNAME_IPADDR, yansipaddr_m},
    {MTNAME_IPPORTS, yansipports_m},
    {MTNAME_BLOCK, yansblock_m},
    {MTNAME_BLOCKS, yansblocks_m},
    {NULL, NULL},
  };

  init_mtable(L, mt);
  luaL_newlib(L, ip_f);
  return 1;
}

int luaopen_eth(lua_State *L) {
  struct mtable mt[] = {
    {MTNAME_ETHADDR, yansethaddr_m},
    {NULL, NULL},
  };

  init_mtable(L, mt);
  luaL_newlib(L, eth_f);
  return 1;
}

