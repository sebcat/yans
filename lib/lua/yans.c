/* vim: set tabstop=2 shiftwidth=2 expandtab ai: */
#include <string.h>

#include <pcap/pcap.h>

#include <lib/net/ip.h>
#include <lib/net/url.h>
#include <lib/net/eth.h>
#include <lib/net/punycode.h>
#include <lib/lua/yans.h>

#define MTNAME_IPADDR     "yans.IPAddr"
#define MTNAME_ETHADDR    "yans.EthAddr"
#define MTNAME_BLOCK      "yans.IPBlock"
#define MTNAME_URLBUILDER "yans.URLBuilder"

#define checkipaddr(L, i) \
    ((ip_addr_t*)luaL_checkudata(L, (i), MTNAME_IPADDR))

#define checkethaddr(L, i) \
    ((struct eth_addr *)luaL_checkudata(L, (i), MTNAME_ETHADDR))

#define checkblock(L, i) \
    ((ip_block_t*)luaL_checkudata(L, (i), MTNAME_BLOCK))

#define checkurlbuilder(L, i) \
    (*(url_ctx_t**)luaL_checkudata(L, (i), MTNAME_URLBUILDER))


static inline ip_addr_t *l_newipaddr(lua_State *L) {
  ip_addr_t *addr;

  addr = lua_newuserdata(L, sizeof(ip_addr_t));
  luaL_setmetatable(L, MTNAME_IPADDR);
  return addr;
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
  const char *addr = luaL_checklstring(L, 1, &len);
  if (len != 6) {
    return luaL_error(L, "EthAddr: invalid address length (%zu)", len);
  }
  eth = l_newethaddr(L);
  eth_addr_init_bytes(eth, addr);
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
  if (ip_block(blk, s, &err) < 0) {
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
  if (ip_block_str(addr, addrbuf, sizeof(addrbuf), &err) < 0) {
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
  int ret;

  blk = checkblock(L, 1);
  addr = checkipaddr(L, 2);
  if ((ret = ip_block_contains(blk, addr, NULL)) < 0) {
    luaL_error(L, "contains: incompatible address types");
  }

  lua_pushboolean(L, ret);
  return 1;
}

/* returns the number of device addresses, and if > 0 a table on TOS */
static lua_Integer l_device_addrs(lua_State*L, pcap_addr_t *addrs) {
  pcap_addr_t *curr;
  lua_Integer naddrs = 0;
  ip_addr_t *addr = NULL, *netmask = NULL;
  ip_block_t *blk;
  struct eth_addr *eth;
  int eth_valid_res;


  for (curr = addrs; curr != NULL; curr = curr->next) {
    if (curr->addr == NULL) {
      continue;
    }
    if ((eth_valid_res = eth_addr_valid(curr->addr)) == ETHERR_OK ||
        (curr->addr->sa_family == AF_INET ||
        curr->addr->sa_family == AF_INET6)) {
      if (naddrs == 0) {
        lua_newtable(L);
      }
    } else {
      continue;
    }

    if (eth_valid_res == ETHERR_OK) {
      lua_newtable(L);
      eth = l_newethaddr(L);
      eth_addr_init(eth, curr->addr);
      lua_setfield(L, -2, "addr");
      naddrs++;
    } else {
      /* AF_INET || AF_INET6 */
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
        blk = l_newipblock(L);
        if (ip_block_netmask(blk, addr, netmask, NULL) == 0) {
          lua_setfield(L, -2, "block");
        } else {
          lua_pop(L, 1);
        }
      }
    }
    lua_rawseti(L, -2, naddrs);
  }

  return naddrs;
}

static int l_urlbuilder(lua_State *L) {
  struct url_opts opts;
  struct url_ctx_t **ctx;
  int flags = 0;

  if (lua_type(L, 1) == LUA_TTABLE) {
    if(lua_getfield(L, 1, "flags") == LUA_TNUMBER) {
      flags = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
  }
  memset(&opts, 0, sizeof(opts));
  opts.host_normalizer = punycode_encode;
  opts.flags = flags;
  ctx = lua_newuserdata(L, sizeof(url_ctx_t*));
  luaL_setmetatable(L, MTNAME_URLBUILDER);
  *ctx = url_ctx_new(&opts);
  if (*ctx == NULL) {
    return luaL_error(L, "URLBuilder: initialization error");
  }
  return 1;
}

static int l_urlbuilder_gc(lua_State *L) {
  url_ctx_t *ctx = checkurlbuilder(L, 1);
  url_ctx_free(ctx);
  return 0;
}

static int l_urlbuilder_parse(lua_State *L) {
  int ret;
  struct url_parts parts;
  url_ctx_t *ctx = checkurlbuilder(L, 1);
  const char *urlstr = luaL_checkstring(L, 2);
  const struct url_opts *opts = url_ctx_opts(ctx);

  if ((ret = url_parse(ctx, urlstr, &parts)) != EURL_OK) {
    return luaL_error(L, url_errstr(ret));
  }

  lua_newtable(L);
  if ((parts.schemelen) > 0) {
    lua_pushlstring(L, urlstr+parts.scheme, parts.schemelen);
    lua_setfield(L, -2, "scheme");
  }

  if ((parts.authlen) > 0) {
    lua_pushlstring(L, urlstr+parts.auth, parts.authlen);
    lua_setfield(L, -2, "auth");
  }

  if ((parts.flags & URLPART_HAS_USERINFO) != 0) {
    /* we may have a zero length userinfo, and for inverse symmetry between
     * parse and build we need to be able to represent this separately from
     * no userinfo present. That's why we check URLPART_HAS_USERINFO instead
     * of parts.userinfolen */
    lua_pushlstring(L, urlstr+parts.userinfo, parts.userinfolen);
    lua_setfield(L, -2, "userinfo");
  }

  if ((parts.hostlen) > 0) {
    lua_pushlstring(L, urlstr+parts.host, parts.hostlen);
    lua_setfield(L, -2, "host");
  }

  if ((parts.flags & URLPART_HAS_PORT) != 0) {
    /* see userinfo comment above on why we check URLPART_HAS_PORT*/
    lua_pushlstring(L, urlstr+parts.port, parts.portlen);
    lua_setfield(L, -2, "port");
  }

  if ((parts.pathlen) > 0) {
    lua_pushlstring(L, urlstr+parts.path, parts.pathlen);
    lua_setfield(L, -2, "path");
  }

  if ((parts.querylen) > 0) {
    lua_pushlstring(L, urlstr+parts.query, parts.querylen);
    lua_setfield(L, -2, "query");
  } else if ((opts->flags & URLFL_REMOVE_EMPTY_QUERY) == 0 &&
      parts.flags & URLPART_HAS_QUERY) {
    lua_pushstring(L, "");
    lua_setfield(L, -2, "query");
  }

  if ((parts.fragmentlen) > 0) {
    lua_pushlstring(L, urlstr+parts.fragment, parts.fragmentlen);
    lua_setfield(L, -2, "fragment");
  } else if ((opts->flags & URLFL_REMOVE_EMPTY_FRAGMENT) == 0 &&
      parts.flags & URLPART_HAS_FRAGMENT) {
    lua_pushstring(L, "");
    lua_setfield(L, -2, "fragment");
  }

  return 1;
}

static int l_urlbuilder_build(lua_State *L) {
  int ret, has_auth = 0;
  luaL_Buffer b;
  url_ctx_t *ctx = checkurlbuilder(L, 1);
  (void)ctx;
  luaL_checktype(L, 2, LUA_TTABLE);
  luaL_buffinit(L, &b);

  lua_getfield(L, 2, "scheme");
  if (lua_type(L, -1) != LUA_TNIL) {
    if ((ret = url_supported_scheme(lua_tostring(L, -1))) != EURL_OK) {
      return luaL_error(L, url_errstr(ret));
    }
    luaL_addvalue(&b);
    luaL_addstring(&b, ":");
  } else {
    lua_pop(L, 1);
  }

  lua_getfield(L, 2, "port");
  lua_getfield(L, 2, "host");
  lua_getfield(L, 2, "userinfo");
  if (lua_type(L, -2) != LUA_TNIL ||
      lua_type(L, -3) != LUA_TNIL ||
      lua_type(L, -1) != LUA_TNIL) {
    has_auth = 1;
    luaL_addstring(&b, "//");
    if (lua_type(L, -1) != LUA_TNIL) { /* userinfo */
      luaL_addvalue(&b);
      luaL_addchar(&b, '@');
    } else {
      lua_pop(L, 1);
    }
    if (lua_type(L, -1) != LUA_TNIL) { /* host */
      luaL_addvalue(&b);
    } else {
      lua_pop(L, 1);
    }
    if (lua_type(L, -1) != LUA_TNIL) { /* port */
      luaL_addchar(&b, ':');
      luaL_addvalue(&b);
    } else {
      lua_pop(L, 1);
    }
  } else {
    lua_pop(L, 3);
    lua_getfield(L, 2, "auth");
    if (lua_type(L, -1) != LUA_TNIL) {
      has_auth = 1;
      luaL_addstring(&b, "//");
      luaL_addvalue(&b);
    }
  }

  lua_getfield(L, 2, "path");
  if (lua_type(L, -1) != LUA_TNIL) {
    const char *s = lua_tostring(L, -1);
    if (*s != '/' && has_auth) {
      luaL_addchar(&b, '/');
    }
    luaL_addvalue(&b);
  } else {
    lua_pop(L, 1);
  }

  lua_getfield(L, 2, "query");
  if (lua_type(L, -1) != LUA_TNIL) {
    luaL_addchar(&b, '?');
    luaL_addvalue(&b);
  } else {
    lua_pop(L, 1);
  }

  lua_getfield(L, 2, "fragment");
  if (lua_type(L, -1) != LUA_TNIL) {
    luaL_addchar(&b, '#');
    luaL_addvalue(&b);
  } else {
    lua_pop(L, 1);
  }

  luaL_pushresult(&b);
  return 1;
}

static int l_urlbuilder_normalize(lua_State *L) {
  buf_t buf;
  int ret;
  url_ctx_t *ctx = checkurlbuilder(L, 1);
  const char *urlstr = luaL_checkstring(L, 2);
  buf_init(&buf, 1024);
  if ((ret = url_normalize(ctx, urlstr, &buf)) != EURL_OK) {
    buf_cleanup(&buf);
    return luaL_error(L, url_errstr(ret));
  }
  lua_pushlstring(L, buf.data, buf.len-1); /* len includes trailing \0 */
  buf_cleanup(&buf);
  return 1;
}

static int l_urlbuilder_resolve(lua_State *L) {
  buf_t buf;
  int ret;
  url_ctx_t *ctx = checkurlbuilder(L, 1);
  const char *basestr = luaL_checkstring(L, 2);
  const char *refstr = luaL_checkstring(L, 3);
  buf_init(&buf, 1024);
  if ((ret = url_resolve(ctx, basestr, refstr, &buf)) != EURL_OK) {
    buf_cleanup(&buf);
    return luaL_error(L, url_errstr(ret));
  }
  lua_pushlstring(L, buf.data, buf.len-1);
  buf_cleanup(&buf);
  return 1;
}

static void l_device(lua_State *L, pcap_if_t *dev) {
  /*NB: can't call lua_error/luaL_error in here; l_devices must clear state */
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
  pcap_if_t *devs = NULL, *curr;
  char errbuf[PCAP_ERRBUF_SIZE];
  lua_Integer ndevs=0;

  errbuf[0] = '\0';
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

static const struct luaL_Reg yansurlbuilder_m[] = {
  {"parse", l_urlbuilder_parse},
  {"build", l_urlbuilder_build},
  {"normalize", l_urlbuilder_normalize},
  {"resolve", l_urlbuilder_resolve},
  {"__gc", l_urlbuilder_gc},
  {NULL, NULL}
};

static const struct luaL_Reg yans_f[] = {
  {"IPAddr", l_ipaddr},
  {"EthAddr", l_ethaddr},
  {"IPBlock", l_ipblock},
  {"URLBuilder", l_urlbuilder},
  {"devices", l_devices},
  /* TODO: - network(str): CIDR handling for networks. return table with
   *         network, broadcast addrs and range (as an IPBlock) */
  {NULL, NULL}
};

int luaopen_yans(lua_State *L) {
  struct {
    const char *mt;
    const struct luaL_Reg *reg;
  } types[] = {
    {MTNAME_IPADDR, yansipaddr_m},
    {MTNAME_ETHADDR, yansethaddr_m},
    {MTNAME_BLOCK, yansblock_m},
    {MTNAME_URLBUILDER, yansurlbuilder_m},
    {NULL, NULL},
  };
  struct {
    const char *name;
    int value;
  } constants[] = {
    {"URLFL_REMOVE_EMPTY_QUERY", URLFL_REMOVE_EMPTY_QUERY},
    {"URLFL_REMOVE_EMPTY_FRAGMENT", URLFL_REMOVE_EMPTY_FRAGMENT},
    {NULL, 0},
  };
  size_t i;

  for(i=0; types[i].mt != NULL; i++) {
    luaL_newmetatable(L, types[i].mt);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, types[i].reg, 0);
  }

  luaL_newlib(L, yans_f);
  for(i=0; constants[i].name != NULL; i++) {
    lua_pushinteger(L, constants[i].value);
    lua_setfield(L, -2, constants[i].name);
  }

  return 1;
}
