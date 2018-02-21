/* vim: set tabstop=2 shiftwidth=2 expandtab ai: */
#include <string.h>

#include <lib/net/ip.h>
#include <lib/net/ports.h>
#include <lib/net/eth.h>
#include <lib/net/route.h>
#include <lib/net/iface.h>
#include <lib/net/neigh.h>
#include <lib/lua/net.h>

#define MTNAME_IPADDR     "yans.IPAddr"
#define MTNAME_IPPORTS    "yans.IPPorts"
#define MTNAME_IPR4PORTS  "yans.IPR4Ports"
#define MTNAME_ETHADDR    "yans.EthAddr"
#define MTNAME_BLOCK      "yans.IPBlock"
#define MTNAME_BLOCKS     "yans.IPBlocks"
#define MTNAME_R4BLOCKS   "yans.IPR4Blocks"

#define checkipaddr(L, i) \
    ((ip_addr_t*)luaL_checkudata(L, (i), MTNAME_IPADDR))

#define checkipports(L, i) \
    ((struct port_ranges *)luaL_checkudata(L, (i), MTNAME_IPPORTS))

#define checkr4ports(L, i) \
    ((struct port_r4ranges *)luaL_checkudata(L, (i), MTNAME_IPR4PORTS))

#define checkethaddr(L, i) \
    ((struct eth_addr *)luaL_checkudata(L, (i), MTNAME_ETHADDR))

#define checkblock(L, i) \
    ((ip_block_t*)luaL_checkudata(L, (i), MTNAME_BLOCK))

#define checkblocks(L, i) \
    ((struct ip_blocks *)luaL_checkudata(L, (i), MTNAME_BLOCKS))

#define checkr4blocks(L, i) \
    ((struct ip_r4blocks *)luaL_checkudata(L, (i), MTNAME_R4BLOCKS))


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

static inline struct port_r4ranges *l_newipr4ports(lua_State *L) {
  struct port_r4ranges *rs;

  rs = lua_newuserdata(L, sizeof(struct port_r4ranges));
  luaL_setmetatable(L, MTNAME_IPR4PORTS);
  memset(rs, 0, sizeof(*rs));
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

static inline struct ip_r4blocks *l_newipr4blocks(lua_State *L) {
  struct ip_r4blocks *blks;

  blks = lua_newuserdata(L, sizeof(struct ip_r4blocks));
  luaL_setmetatable(L, MTNAME_R4BLOCKS);
  memset(blks, 0, sizeof(*blks));
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

static ip_addr_t *castipaddr(lua_State *L, int index) {
  ip_addr_t *addr;
  const char *str;
  int ret;

  /* is it already the correct type? */
  addr = luaL_testudata(L, index, MTNAME_IPADDR);
  if (addr != NULL) {
    return addr;
  }

  /* replace the value at 'index' with a new port range, or fail */
  str = luaL_checkstring(L, index);
  addr = l_newipaddr(L);
  ret = ip_addr(addr, str, NULL);
  if (ret < 0) {
    luaL_error(L, "invalid IP address");
  }
  lua_replace(L, index);
  return addr;
}

static int l_ipaddr_equals(lua_State *L) {
  ip_addr_t *a;
  ip_addr_t *b;
  int err = 0;
  int ret;

  a = checkipaddr(L, 1);
  b = castipaddr(L, 2);
  ret = ip_addr_cmp(a, b, &err);
  if (err != 0 || ret != 0) {
    lua_pushboolean(L, 0);
  } else {
    lua_pushboolean(L, 1);
  }
  return 1;
}

static int l_ipaddr_compare(lua_State *L) {
  ip_addr_t *a;
  ip_addr_t *b;
  int err = 0;
  lua_Integer ret;

  a = checkipaddr(L, 1);
  b = castipaddr(L, 2);
  ret = (lua_Integer)ip_addr_cmp(a, b, &err);
  if (err != 0) {
    luaL_error(L, "IPAddr: failed comparison: %s\n", ip_addr_strerror(err));
  }
  lua_pushinteger(L, ret);
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

static int l_ipr4blocks_gc(lua_State *L) {
  struct ip_r4blocks *blks = checkr4blocks(L, 1);
  ip_r4blocks_cleanup(blks);
  return 0;
}

static int l_ipblocks_nextr4_iter(lua_State *L) {
  struct ip_r4blocks *iter;
  ip_addr_t *addr;

  iter = checkr4blocks(L, lua_upvalueindex(2));
  addr = l_newipaddr(L);
  return ip_r4blocks_next(iter, addr);
}

static int l_ipblocks_nextr4(lua_State *L) {
  struct ip_blocks *blks;
  struct ip_r4blocks *iter;
  int ret;

  blks = checkblocks(L, 1);
  iter = l_newipr4blocks(L);
  ret = ip_r4blocks_init(iter, blks);
  if (ret < 0) {
    return 0;
  }

  /* we keep blks in the closure for ref */
  lua_pushcclosure(L, l_ipblocks_nextr4_iter, 2);
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

static int l_pushsrcaddr(lua_State *L, struct iface_srcaddr *src) {
  ip_block_t *blk;
  int ret;

  /* create the source address table, {iface, addr, mask} */
  lua_createtable(L, 3, 0);

  /* set the iface */
  lua_pushstring(L, src->ifname);
  lua_rawseti(L, -2, 1);

  /* create and set the address */
  blk = l_newipblock(L);
  ret = ip_block_from_addrs(blk, (ip_addr_t*)&src->addr.u.sa,
      (ip_addr_t*)&src->addr.u.sa, NULL);
  if (ret < 0) {
    lua_pop(L, 2);
    return -1;
  }
  lua_rawseti(L, -2, 2);

  /* create and set the netmask */
  blk = l_newipblock(L);
  ret = ip_block_netmask(blk, (ip_addr_t*)&src->addr.u.sa,
      (ip_addr_t*)&src->mask.u.sa, NULL);
  if (ret < 0) {
    lua_pop(L, 2);
    return -1;
  }
  lua_rawseti(L, -2, 3);

  return 0;
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

/* expects a lua table at TOS */
static void add_routes_to_table(lua_State *L, struct route_table *rt) {
  lua_Integer i;

  /* sub-level table containing a sequence of entries */
  lua_createtable(L, rt->nentries_ip4, 0);
  for (i = 0; i < rt->nentries_ip4; i++) {
    lua_newtable(L); /* a single routing table entry */
    if (l_addroute(L, &rt->entries_ip4[i]) < 0) {
      lua_pop(L, 1);
      continue;
    }
    lua_rawseti(L, -2, i+1);
  }
  lua_setfield(L, -2, "ip4_routes");

  lua_createtable(L, rt->nentries_ip6, 0);
  for (i = 0; i < rt->nentries_ip6; i++) {
    lua_newtable(L); /* a single routing table entry */
    if (l_addroute(L, &rt->entries_ip6[i]) < 0) {
      lua_pop(L, 1);
      continue;
    }
    lua_rawseti(L, -2, i+1);
  }
  lua_setfield(L, -2, "ip6_routes");
}

static int l_addneigh4(lua_State *L, const struct neigh_ip4_entry *e) {
  struct eth_addr *eth;

  if (e->iface[0] == '\0' || memcmp(e->hwaddr, "\0\0\0\0\0\0", 6) == 0) {
    /* skip empty hwaddrs or iface addrs */
    return -1;
  }

  eth = l_newethaddr(L);
  eth_addr_init_bytes(eth, e->hwaddr);
  lua_setfield(L, -2, "hwaddr");
  lua_pushstring(L, e->iface);
  lua_setfield(L, -2, "iface");
  l_pushipaddr(L, (struct sockaddr *)&e->sin);
  lua_setfield(L, -2, "ipaddr");
  return 0;
}

static int l_addneigh6(lua_State *L, const struct neigh_ip6_entry *e) {
  struct eth_addr *eth;

  if (e->iface[0] == '\0' || memcmp(e->hwaddr, "\0\0\0\0\0\0", 6) == 0) {
    /* skip empty hwaddrs or iface addrs */
    return -1;
  }

  eth = l_newethaddr(L);
  eth_addr_init_bytes(eth, e->hwaddr);
  lua_setfield(L, -2, "hwaddr");
  lua_pushstring(L, e->iface);
  lua_setfield(L, -2, "iface");
  l_pushipaddr(L, (struct sockaddr *)&e->sin6);
  lua_setfield(L, -2, "ipaddr");
  return 0;
}

static void add_neigh_to_table(lua_State *L,
    const struct neigh_ip4_entry *ip4_neigh, size_t nip4_neigh,
    const struct neigh_ip6_entry *ip6_neigh, size_t nip6_neigh) {
  size_t i;

  if (nip4_neigh > 0) {
    lua_createtable(L, nip4_neigh, 0);
    for (i = 0; i < nip4_neigh; i++) {
      lua_newtable(L); /* a single neighbor table entry */
      if (l_addneigh4(L, &ip4_neigh[i]) < 0) {
        lua_pop(L, 1);
        continue;
      }
      lua_rawseti(L, -2, i+1);
    }
    lua_setfield(L, -2, "ip4_neigh");
  }

  if (nip6_neigh > 0) {
    lua_createtable(L, nip6_neigh, 0);
    for (i = 0; i < nip6_neigh; i++) {
      lua_newtable(L); /* a single neighbor table entry */
      if (l_addneigh6(L, &ip6_neigh[i]) < 0) {
        lua_pop(L, 1);
        continue;
      }
      lua_rawseti(L, -2, i+1);
    }
    lua_setfield(L, -2, "ip6_neigh");
  }

}

static int l_routes(lua_State *L) {
  struct route_table rt;
  char errbuf[128];

  if (route_table_init(&rt) < 0) {
    route_table_strerror(&rt, errbuf, sizeof(errbuf));
    return luaL_error(L, "route_table_init: %s", errbuf);
  }

  lua_createtable(L, 0, 2); /* top-level table containing ip4, ip6 keys */
  add_routes_to_table(L, &rt);
  route_table_cleanup(&rt);
  return 1;
}

static int l_neighbors(lua_State *L) {
  struct neigh_ip4_entry *ip4_neigh = NULL;
  struct neigh_ip6_entry *ip6_neigh = NULL;
  size_t nip4_neigh = 0;
  size_t nip6_neigh = 0;

  ip4_neigh = neigh_get_ip4_entries(&nip4_neigh, NULL);
  ip6_neigh = neigh_get_ip6_entries(&nip6_neigh, NULL);
  lua_createtable(L, 0, 2);
  add_neigh_to_table(L, ip4_neigh, nip4_neigh, ip6_neigh, nip6_neigh);
  if (ip4_neigh) {
    neigh_free_ip4_entries(ip4_neigh);
  }

  if (ip6_neigh) {
    neigh_free_ip6_entries(ip6_neigh);
  }

  return 1;
}

static void add_ifaces_to_table(lua_State *L, struct iface_entries *ifs) {
  struct iface_entry *ent;
  struct eth_addr *eth;
  int i;
  int ret;

  lua_newtable(L);
  for (i = 0; i < ifs->nifaces; i++) {
    ent = &ifs->ifaces[i];
    lua_createtable(L, 0, 5);
    eth = l_newethaddr(L);
    eth_addr_init_bytes(eth, ent->addr);
    lua_setfield(L, -2, "addr");
    lua_pushinteger(L, (lua_Integer)ent->index);
    lua_setfield(L, -2, "index");
    lua_pushstring(L, ent->name);
    lua_setfield(L, -2, "name");
    lua_pushboolean(L, ent->flags & IFACE_UP);
    lua_setfield(L, -2, "up");
    lua_pushboolean(L, ent->flags & IFACE_LOOPBACK);
    lua_setfield(L, -2, "loopback");
    lua_rawseti(L, -2, (lua_Integer)i+1);
  }
  lua_setfield(L, -2, "ifaces");

  lua_createtable(L, ifs->nipsrcs, 0);
  for (i = 0; i < ifs->nipsrcs; i++) {
    ret = l_pushsrcaddr(L, &ifs->ipsrcs[i]);
    if (ret == 0) {
      lua_rawseti(L, -2, (lua_Integer)i+1);
    }
  }
  lua_setfield(L, -2, "ip_srcs");
}

static int l_ifaces(lua_State *L) {
  struct iface_entries ifs;

  if (iface_init(&ifs) < 0) {
    return luaL_error(L, "iface_init: %s", iface_strerror(&ifs));
  }

  lua_newtable(L);
  add_ifaces_to_table(L, &ifs);
  iface_cleanup(&ifs);
  return 1;
}

/* expects a lua table with the keys
 *   - ifaces
 *   - ip_srcs
 *   - ip4_routes
 *   - ip4_neigh
 *   - ip6_routes
 *   - ip6_neigh
 * which contains route_table_entry, iface_entries data. The unmarshaled
 * data is returned in a table.  */
static int l_unmarshal_routes(lua_State *L) {
  struct route_table rt = {0};
  struct iface_entries ifs = {0};
  size_t ip6_route_sz = 0;
  size_t ip6_neigh_sz = 0;
  size_t ip4_route_sz = 0;
  size_t ip_src_sz = 0;
  size_t ip4_neigh_sz = 0;
  size_t ifaces_sz = 0;
  const char *ip6_route_data = NULL;
  const char *ip6_neigh_data = NULL;
  const char *ip4_route_data = NULL;
  const char *ip4_neigh_data = NULL;
  const char *ip_src_data = NULL;
  const char *ifaces_data = NULL;
  int t;

  luaL_checktype(L, 1, LUA_TTABLE);

  t = lua_getfield(L, 1, "ip6_routes");
  if (t == LUA_TSTRING) {
    ip6_route_data = lua_tolstring(L, -1, &ip6_route_sz);
  }
  lua_pop(L, 1);

  t = lua_getfield(L, 1, "ip6_neigh");
  if (t == LUA_TSTRING) {
    ip6_neigh_data = lua_tolstring(L, -1, &ip6_neigh_sz);
  }
  lua_pop(L, 1);

  t = lua_getfield(L, 1, "ip4_routes");
  if (t == LUA_TSTRING) {
    ip4_route_data = lua_tolstring(L, -1, &ip4_route_sz);
  }
  lua_pop(L, 1);

  t = lua_getfield(L, 1, "ip4_neigh");
  if (t == LUA_TSTRING) {
    ip4_neigh_data = lua_tolstring(L, -1, &ip4_neigh_sz);
  }
  lua_pop(L, 1);

  t = lua_getfield(L, 1, "ip_srcs");
  if (t == LUA_TSTRING) {
    ip_src_data = lua_tolstring(L, -1, &ip_src_sz);
  }
  lua_pop(L, 1);

  t = lua_getfield(L, 1, "ifaces");
  if (t == LUA_TSTRING) {
    ifaces_data = lua_tolstring(L, -1, &ifaces_sz);
  }
  lua_pop(L, 1);

  if (ip6_route_sz > 0 &&
      (ip6_route_sz % sizeof(struct route_table_entry)) == 0) {
    rt.entries_ip6 = (struct route_table_entry *)ip6_route_data;
    rt.nentries_ip6 = ip6_route_sz / sizeof(struct route_table_entry);
  }

  if (ip4_route_sz > 0 &&
      (ip4_route_sz % sizeof(struct route_table_entry)) == 0) {
    rt.entries_ip4 = (struct route_table_entry *)ip4_route_data;
    rt.nentries_ip4 = ip4_route_sz / sizeof(struct route_table_entry);
  }

  if (ip_src_sz > 0 &&
      (ip_src_sz % sizeof(struct iface_srcaddr)) == 0) {
    ifs.ipsrcs = (struct iface_srcaddr*)ip_src_data;
    ifs.nipsrcs = ip_src_sz / sizeof(struct iface_srcaddr);
  }

  if (ifaces_sz > 0 && ifaces_sz % sizeof(struct iface_entry) == 0) {
    ifs.ifaces = (struct iface_entry *)ifaces_data;
    ifs.nifaces = ifaces_sz / sizeof(struct iface_entry);
  }

  lua_createtable(L, 0, 5);
  add_routes_to_table(L, &rt);
  add_neigh_to_table(L,
      (const struct neigh_ip4_entry*)ip4_neigh_data,
      ip4_neigh_sz / sizeof(struct neigh_ip4_entry),
      (const struct neigh_ip6_entry*)ip6_neigh_data,
      ip6_neigh_sz / sizeof(struct neigh_ip6_entry));
  add_ifaces_to_table(L, &ifs);
  /* rt, ifs is not cleaned up, because Lua owns the memory */
  return 1;
}

static int l_ipr4ports_gc(lua_State *L) {
  struct port_r4ranges *r4 = checkr4ports(L, 1);
  port_r4ranges_cleanup(r4);
  return 0;
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

static struct port_ranges *castipports(lua_State *L, int index) {
  struct port_ranges *rs;
  const char *str;
  int ret;

  /* is it already the correct type? */
  rs = luaL_testudata(L, index, MTNAME_IPPORTS);
  if (rs != NULL) {
    return rs;
  }

  /* replace the value at 'index' with a new port range, or fail */
  str = luaL_checkstring(L, index);
  rs = l_newipports(L);
  ret = port_ranges_from_str(rs, str, NULL);
  if (ret < 0) {
    luaL_error(L, "invalid port range");
  }
  lua_replace(L, index);
  return rs;
}

static int l_ipports_add(lua_State *L) {
  int ret;
  struct port_ranges *dst = checkipports(L, 1);
  struct port_ranges *from = castipports(L, 2);

  ret = port_ranges_add(dst, from);
  if (ret < 0) {
    return luaL_error(L, "ipports_add: memory allocation failure");
  }

  lua_pop(L, 1);
  return 1;
}

static int l_ipports_next_iter(lua_State *L) {
  struct port_ranges *rs = checkipports(L, lua_upvalueindex(1));
  uint16_t port;

  if (port_ranges_next(rs, &port) > 0) {
    lua_pushinteger(L, (lua_Integer)port);
    return 1;
  }

  return 0;
}

static int l_ipports_next(lua_State *L) {
  checkipports(L, 1);
  lua_pushcclosure(L, l_ipports_next_iter, 1);
  return 1;
}

static int l_ipports_nextr4_iter(lua_State *L) {
  struct port_r4ranges *r4;
  uint16_t port;

  r4 = checkr4ports(L, lua_upvalueindex(2));
  if (port_r4ranges_next(r4, &port) > 0) {
    lua_pushinteger(L, (lua_Integer)port);
    return 1;
  }

  return 0;
}

static int l_ipports_nextr4(lua_State *L) {
  struct port_ranges *rs;
  struct port_r4ranges *r4;
  int ret;

  rs = checkipports(L, 1);
  r4 = l_newipr4ports(L);
  ret = port_r4ranges_init(r4, rs);
  if (ret < 0) {
    return 0;
  }

  lua_pushcclosure(L, l_ipports_nextr4_iter, 2);
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

  /* immutable arithmetic (evals to a copy containing the result) */
  {"__add", l_ipaddr_addop},
  {"__sub", l_ipaddr_subop},

  {"equals", l_ipaddr_equals},
  {"compare", l_ipaddr_compare},
  {"add", l_ipaddr_add},
  {"sub", l_ipaddr_sub},
  {NULL, NULL}
};

static const struct luaL_Reg yansipports_m[] = {
  {"__tostring", l_ipports_tostring},
  {"__gc", l_ipports_gc},
  {"next", l_ipports_next},
  {"nextr4", l_ipports_nextr4},
  {"add", l_ipports_add},
  {NULL, NULL},
};

static const struct luaL_Reg yansr4ports_m[] = {
  {"__gc", l_ipr4ports_gc},
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
  {"nextr4", l_ipblocks_nextr4},
  {"contains", l_ipblocks_contains},
  {NULL, NULL},
};

static const struct luaL_Reg yansr4blocks_m[] = {
  {"__gc", l_ipr4blocks_gc},
  {NULL, NULL},
};

static const struct luaL_Reg ip_f[] = {
  {"addr", l_ipaddr},
  {"addrs", l_ipblocks},
  {"block", l_ipblock},
  {"ports", l_ipports},
  {"routes", l_routes},
  {"neighbors", l_neighbors},
  {"unmarshal_routes", l_unmarshal_routes},
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
    {MTNAME_IPR4PORTS, yansr4ports_m},
    {MTNAME_BLOCK, yansblock_m},
    {MTNAME_BLOCKS, yansblocks_m},
    {MTNAME_R4BLOCKS, yansr4blocks_m},
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

