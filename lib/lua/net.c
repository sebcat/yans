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
#define MTNAME_BLOCK      "yans.IPBlock"
#define MTNAME_BLOCKS     "yans.IPBlocks"
#define MTNAME_R4BLOCKS   "yans.IPR4Blocks"

#define checkipaddr(L, i) \
    ((ip_addr_t*)luaL_checkudata(L, (i), MTNAME_IPADDR))

#define checkipports(L, i) \
    ((struct port_ranges *)luaL_checkudata(L, (i), MTNAME_IPPORTS))

#define checkr4ports(L, i) \
    ((struct port_r4ranges *)luaL_checkudata(L, (i), MTNAME_IPR4PORTS))

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

static inline ip_addr_t *l_pushipaddr(lua_State *L,
    const struct sockaddr *saddr) {
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

static int l_ipaddr_version(lua_State *L) {
  ip_addr_t *addr;
  int af;

  addr = checkipaddr(L, 1);
  af = addr->u.sa.sa_family;
  if (af == AF_INET) {
    lua_pushinteger(L, 4);
  } else if (af == AF_INET6) {
    lua_pushinteger(L, 6);
  } else {
    return luaL_error(L, "unknown address family: %d", af);
  }

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

static int l_ipblock_version(lua_State *L) {
  ip_block_t *blk;
  int af;

  blk = checkblock(L, 1);
  af = blk->first.u.sa.sa_family;
  if (af == AF_INET) {
    lua_pushinteger(L, 4);
  } else if (af == AF_INET6) {
    lua_pushinteger(L, 6);
  } else {
    return luaL_error(L, "invalid block family: %d", af);
  }

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

static int l_ipblocks_nextblock_iter(lua_State *L) {
  struct ip_blocks *blks;
  ip_block_t *blk;
  ip_block_t *curr;
  int ret;
  int err = 0;

  blks = checkblocks(L, lua_upvalueindex(1));
  if (blks->curr_block >= blks->nblocks) {
    return 0;
  }

  blk = l_newipblock(L);
  curr = &blks->blocks[blks->curr_block];
  ret = ip_block_from_addrs(blk, &curr->first, &curr->last, &err);
  if (ret < 0) {
    return luaL_error(L, "block iterator failure: %s", ip_block_strerror(err));
  }

  blks->curr_block++;
  return 1;
}

static int l_ipblocks_nextblock(lua_State *L) {
  struct ip_blocks *blks;

  blks = checkblocks(L, 1);
  if (blks->nblocks == 0) {
    return 0;
  }

  blks->curr_block = 0;
  lua_pushcclosure(L, l_ipblocks_nextblock_iter, 1);
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
  l_pushipaddr(L, &src->addr.u.sa);
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
    ret = ip_block_from_addrs(blk, &ent->addr, &ent->addr, NULL);
    if (ret < 0) {
      lua_pop(L, 1);
      return -1;
    } else {
      lua_setfield(L, -2, "block");
    }
  } else if (ip_block_netmask(blk, &ent->addr,&ent->mask, NULL) == 0) {
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
    l_pushipaddr(L, &ent->gw.u.sa);
    lua_setfield(L, -2, "gw");
  }

  lua_pushinteger(L, (lua_Integer)ent->ifindex);
  lua_setfield(L, -2, "ifindex");

  return 0;
}

static int l_ethbytestostr(lua_State *L, const char *bytes) {
  char addr[24];
  const unsigned char *bs = (const unsigned char*)bytes;

  snprintf(addr, sizeof(addr), "%02x:%02x:%02x:%02x:%02x:%02x",
      (unsigned int)bs[0], (unsigned int)bs[1], (unsigned int)bs[2],
      (unsigned int)bs[3], (unsigned int)bs[4], (unsigned int)bs[5]);
  lua_pushstring(L, addr);
  return 1;
}

static int l_addneigh(lua_State *L, const struct neigh_entry *e) {
  if (e->iface[0] == '\0' || memcmp(e->hwaddr, "\0\0\0\0\0\0", 6) == 0) {
    /* skip empty hwaddrs or iface addrs */
    return -1;
  }

  l_ethbytestostr(L, e->hwaddr);
  lua_setfield(L, -2, "hwaddr");
  lua_pushstring(L, e->iface);
  lua_setfield(L, -2, "iface");
  l_pushipaddr(L, &e->ipaddr.u.sa);
  lua_setfield(L, -2, "ipaddr");
  return 0;
}

static int l_routes(lua_State *L) {
  struct route_table rt;
  char errbuf[128];
  size_t i;

  if (route_table_init(&rt) < 0) {
    route_table_strerror(&rt, errbuf, sizeof(errbuf));
    return luaL_error(L, "route_table_init: %s", errbuf);
  }

  if (rt.nentries > 0) {
    lua_createtable(L, rt.nentries, 0);
    for (i = 0; i < rt.nentries; i++) {
      lua_newtable(L); /* a single routing table entry */
      if (l_addroute(L, &rt.entries[i]) < 0) {
        lua_pop(L, 1);
        continue;
      }
      lua_seti(L, -2, (lua_Integer)i+1);
    }
  } else {
    lua_pushnil(L);
  }
  route_table_cleanup(&rt);
  return 1;
}

static int l_neighbors(lua_State *L) {
  struct neigh_entry *ip_neigh = NULL;
  size_t nip_neigh = 0;
  size_t i;

  ip_neigh = neigh_get_entries(&nip_neigh, NULL);
  if (ip_neigh != NULL) {
    lua_createtable(L, nip_neigh, 0);
    for (i = 0; i < nip_neigh; i++) {
      lua_newtable(L); /* a single neighbor table entry */
      if (l_addneigh(L, &ip_neigh[i]) < 0) {
        lua_pop(L, 1);
        continue;
      }
      lua_seti(L, -2, (lua_Integer)i+1);
    }
    neigh_free_entries(ip_neigh);
  } else {
    lua_pushnil(L);
  }

  return 1;
}

static void add_ifaces_to_table(lua_State *L, struct iface_entries *ifs) {
  struct iface_entry *ent;
  int i;
  int ret;

  lua_newtable(L);
  for (i = 0; i < ifs->nifaces; i++) {
    ent = &ifs->ifaces[i];
    lua_createtable(L, 0, 5);
    l_ethbytestostr(L, ent->addr);
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

struct netconf_data {
  struct route_table rt;
  struct iface_entries ifs;
  struct neigh_entry *neighs;
  size_t nneighs;
};

static void l_nc_ethaddr(lua_State *L, struct iface_entry *ifent) {
  l_ethbytestostr(L, ifent->addr);
  lua_setfield(L, -2, "ethaddr");
}

static void l_nc_neigh(lua_State *L, struct netconf_data *cfg,
    struct iface_entry *ifent) {
  size_t i;
  struct neigh_entry *curr;
  char addrbuf[64];
  int ret;

  lua_newtable(L);
  for (i = 0; i < cfg->nneighs; i++) {
    curr = &cfg->neighs[i];
    /* skip this one if it's not the current interface, or if the neighbor is
     * the the current interface itself (same address) */
    if (strcmp(curr->iface, ifent->name) != 0 ||
        memcmp(curr->hwaddr, ifent->addr, ETH_ALEN) == 0) {
      continue;
    }

    /* convert the IP address to a string for use as table key */
    ret = ip_addr_str(&curr->ipaddr, addrbuf, sizeof(addrbuf), NULL);
    if (ret < 0) {
      continue;
    }

    /* insert the hwaddr as the table value */
    l_ethbytestostr(L, curr->hwaddr);
    lua_setfield(L, -2, addrbuf);
  }
  lua_setfield(L, -2, "ip_neigh");
}

static void l_nc_srcs(lua_State *L, struct netconf_data *cfg,
    struct iface_entry *ifent, size_t *nbr_sources) {
  size_t i;
  struct iface_srcaddr *curr;
  ip_block_t *blk;
  int ret;
  lua_Integer ipos = 1;
  size_t nsrcs = 0;

  lua_newtable(L);
  for (i = 0; i < cfg->ifs.nipsrcs; i++) {
    curr = &cfg->ifs.ipsrcs[i];

    /* skip sources not belonging to the current interface */
    if (strcmp(curr->ifname, ifent->name) != 0) {
      continue;
    }

    /* create current entry */
    lua_createtable(L, 0, 2);

    /* setup addr field */
    l_pushipaddr(L, &curr->addr.u.sa);
    lua_setfield(L, -2, "addr");

    /* setup subnet field */
    blk = l_newipblock(L);
    ret = ip_block_netmask(blk, &curr->addr, &curr->mask, NULL);
    if (ret < 0) {
      lua_pop(L, 2); /* pop block and table (addr already in table) */
      continue;
    }
    lua_setfield(L, -2, "subnet");

    /* add current entry to ip_srcs table */
    lua_seti(L, -2, ipos++);
    nsrcs++;
  }

  lua_setfield(L, -2, "ip_srcs"); /* add ip_srcs table to netconf[iface] */
  if (nbr_sources != NULL) {
    *nbr_sources = nsrcs;
  }
}

static void l_nc_routes(lua_State *L, struct netconf_data *cfg,
    struct iface_entry *ifent) {
  lua_Integer gwpos = 1;
  lua_Integer dstpos = 1;
  struct route_table_entry *curr;
  ip_block_t *blk;
  size_t i;
  int ret;

  lua_newtable(L); /* ip_gws */
  lua_newtable(L); /* ip_dsts */
  for (i = 0; i < cfg->rt.nentries; i++) {
    curr = &cfg->rt.entries[i];
    if (!(curr->flags & RTENTRY_UP)) {
      /* skip inactive routes */
      continue;
    }

    if (curr->gw_ifindex == ifent->index) {
      /* this route has it's destination on the network of the current
       * interface - add it to ip_dsts */
      blk = l_newipblock(L);
      ret = ip_block_netmask(blk, &curr->addr, &curr->mask, NULL);
      if (ret < 0) {
        lua_pop(L, 1);
        continue;
      }
      lua_seti(L, -2, dstpos++); /* add to ip_dsts */
    } else if (curr->gw_ifindex < 0 && curr->ifindex == ifent->index) {
      /* this route has a gateway on the network of the current interface -
       * add it to ip_gws as a {addr=...,subnet=...} entry */
      lua_createtable(L, 0, 2); /* create current entry */
      /* setup addr field */
      l_pushipaddr(L, &curr->gw.u.sa);
      lua_setfield(L, -2, "addr");
      /* setup subnet field */
      blk = l_newipblock(L);
      ret = ip_block_netmask(blk, &curr->addr, &curr->mask, NULL);
      if (ret < 0) {
        lua_pop(L, 2); /* pop block and table (addr already in table) */
        continue;
      }
      lua_setfield(L, -2, "subnet");
      lua_seti(L, -3, gwpos++); /* add to ip_gws */
    }
  }

  lua_setfield(L, -3, "ip_dsts");
  lua_setfield(L, -2, "ip_gws");
}

/* assumes table at TOS where the netconf entries will be inserted with
 * the interface name as key, and netconf table as value */
static void l_mknetconf(lua_State *L, struct netconf_data *cfg) {
  size_t i;
  struct iface_entry *curr;
  size_t nsrcs = 0;

  for (i = 0; i < cfg->ifs.nifaces; i++) {
    curr = &cfg->ifs.ifaces[i];

    /* skip inactive and loopback interfaces */
    if (!(curr->flags & IFACE_UP) || curr->flags & IFACE_LOOPBACK) {
      continue;
    }

    /* NB: this means that for every iface we will iterate over all
     * neighs, srcs, dsts and gws. We could do an insertion of all ifaces
     * first, and then do a table lookup for each thing we want to add for
     * an iface, but that's probably only worth it if N is big. We should
     * only fetch the nc once per process, and the number of interfaces
     * should be relatively small. For some networks, the neighbor table
     * can be huge though. */

    lua_newtable(L);

    l_nc_srcs(L, cfg, curr, &nsrcs);
    if (nsrcs == 0) {
      /* Currently, there's no use case to handle interfaces without configured
       * source addresses. If there ever is one, we can always add flags to
       * control this behavior, but always skip them for now. */
      lua_pop(L, 1);
      continue;
    }

    l_nc_ethaddr(L, curr);
    l_nc_neigh(L, cfg, curr);
    l_nc_routes(L, cfg, curr);
    lua_setfield(L, -2, curr->name);
  }
}

/* expects a lua table with the keys
 *   - ifaces
 *   - ip_srcs
 *   - ip_neigh
 *   - ip_routes
 * which contains binary data used to build the netconf entries. The
 * unmarshaled * data is returned in a table.  */
static int l_unmarshal_netconf(lua_State *L) {
  struct netconf_data cfg = {{0}};
  size_t ip_route_sz = 0;
  size_t ip_src_sz = 0;
  size_t ip_neigh_sz = 0;
  size_t ifaces_sz = 0;
  const char *ip_route_data = NULL;
  const char *ip_neigh_data = NULL;
  const char *ip_src_data = NULL;
  const char *ifaces_data = NULL;
  int t;

  luaL_checktype(L, 1, LUA_TTABLE);

  t = lua_getfield(L, 1, "ip_routes");
  if (t == LUA_TSTRING) {
    ip_route_data = lua_tolstring(L, -1, &ip_route_sz);
  }
  lua_pop(L, 1);

  t = lua_getfield(L, 1, "ip_neigh");
  if (t == LUA_TSTRING) {
    ip_neigh_data = lua_tolstring(L, -1, &ip_neigh_sz);
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

  if (ip_route_sz > 0) {
    cfg.rt.entries = (struct route_table_entry *)ip_route_data;
    cfg.rt.nentries = ip_route_sz / sizeof(struct route_table_entry);
  }

  if (ip_src_sz > 0) {
    cfg.ifs.ipsrcs = (struct iface_srcaddr*)ip_src_data;
    cfg.ifs.nipsrcs = ip_src_sz / sizeof(struct iface_srcaddr);
  }

  if (ifaces_sz > 0) {
    cfg.ifs.ifaces = (struct iface_entry *)ifaces_data;
    cfg.ifs.nifaces = ifaces_sz / sizeof(struct iface_entry);
  }

  if (ip_neigh_sz > 0) {
    cfg.neighs = (struct neigh_entry *)ip_neigh_data;
    cfg.nneighs = ip_neigh_sz / sizeof(struct neigh_entry);
  }

  lua_newtable(L);
  l_mknetconf(L, &cfg);
  /* cfg is not cleaned up - Lua owns the memory */
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

static int l_ipports_add_ranges(lua_State *L) {
  int ret;
  struct port_ranges *dst = checkipports(L, 1);
  struct port_ranges *from = castipports(L, 2);

  ret = port_ranges_add_ranges(dst, from);
  if (ret < 0) {
    return luaL_error(L, "ipports_add_ranges: memory allocation failure");
  }

  lua_pop(L, 1);
  return 1;
}

static int l_ipports_add_port(lua_State *L) {
  int ret;
  lua_Integer port;
  struct port_ranges *dst;

  dst = checkipports(L, 1);
  port = luaL_checkinteger(L, 2);
  if (port < 0 || port > 65535) {
    return luaL_error(L, "ipports_add_port: port %d out of range", port);
  }

  ret = port_ranges_add_port(dst, (uint16_t)port);
  if (ret < 0) {
    return luaL_error(L, "ipports_add_ranges: memory allocation failure");
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
  {"version", l_ipaddr_version},
  {NULL, NULL}
};

static const struct luaL_Reg yansipports_m[] = {
  {"__tostring", l_ipports_tostring},
  {"__gc", l_ipports_gc},
  {"next", l_ipports_next},
  {"nextr4", l_ipports_nextr4},
  {"add_ranges", l_ipports_add_ranges},
  {"add_port", l_ipports_add_port},
  {NULL, NULL},
};

static const struct luaL_Reg yansr4ports_m[] = {
  {"__gc", l_ipr4ports_gc},
  {NULL, NULL},
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
  {"version", l_ipblock_version},
  {NULL, NULL}
};

static const struct luaL_Reg yansblocks_m[] = {
  {"__tostring", l_ipblocks_tostring},
  {"__gc", l_ipblocks_gc},
  {"next", l_ipblocks_next},
  {"nextr4", l_ipblocks_nextr4},
  {"next_block", l_ipblocks_nextblock},
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
  {"unmarshal_netconf", l_unmarshal_netconf},
  {NULL, NULL},
};

static const struct luaL_Reg eth_f[] = {
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
  luaL_newlib(L, eth_f);
  return 1;
}

