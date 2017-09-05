#include <stdlib.h>
#include <string.h>

#include <lib/util/eds.h>
#include <lib/lua/eds.h>

#define MTNAME_LDSSERVICES "yans.LDSServices"
#define MTNAME_LDSCLIENT "yans.LDSClient"

struct lds_services {
  lua_State *L;
  size_t nsvcs;
  int *tblrefs; /* array of luaL_ref'd service tables */
  struct eds_service svcs[];
};

struct lds_client {
  int tblref; /* client state table in Lua registry */
  int selfref; /* userdata reference in Lua registry */
  struct eds_client *self;
  lua_State *L;
};

#define checkldsservices(L, i) \
    ((struct lds_services *)luaL_checkudata(L, (i), MTNAME_LDSSERVICES))

#define checkldsclient(L, i) \
    ((struct lds_client *)luaL_checkudata(L, (i), MTNAME_LDSCLIENT))

static int l_edsnsvcs(lua_State *L) {
  struct lds_services *svcs = checkldsservices(L, 1);
  lua_pushinteger(L, (lua_Integer)svcs->nsvcs);
  return 1;
}

static int l_edsserve(lua_State *L) {
  struct lds_services *svcs = checkldsservices(L, 1);
  if (eds_serve(svcs->svcs) < 0) {
    return luaL_error(L, "eds_serve failure");
  }

  return 0;
}

static int l_edsservesinglebyname(lua_State *L) {
  struct lds_services *svcs = checkldsservices(L, 1);
  const char *name = luaL_checkstring(L, 2);
  if (eds_serve_single_by_name(svcs->svcs, name) < 0) {
    return luaL_error(L, "eds_serve_single_by_name failure");
  }
  return 0;
}

static int l_edsgc(lua_State *L) {
  size_t i;
  struct lds_services *svcs = checkldsservices(L, 1);
  if (svcs != NULL) {
    for (i = 0; i < svcs->nsvcs; i++) {
      if (svcs->svcs[i].name != NULL) {
        free((char*)svcs->svcs[i].name);
      }
      if (svcs->svcs[i].path != NULL) {
        free((char*)svcs->svcs[i].path);
      }
      if (svcs->tblrefs[i] != LUA_REFNIL) {
        luaL_unref(L, LUA_REGISTRYINDEX, svcs->tblrefs[i]);
      }
    }
    if (svcs->tblrefs != NULL) {
      free(svcs->tblrefs);
    }
  }
  return 0;
}

static inline struct lds_client *get_lds_client(struct eds_client *cli) {
  struct lds_client **lds_cli = (struct lds_client **)cli->udata;
  return *lds_cli;
}

static int l_clidata(lua_State *L) {
  struct lds_client *lds_cli = checkldsclient(L, 1);
  lua_rawgeti(L, LUA_REGISTRYINDEX, lds_cli->tblref);
  return 1;
}

static int l_cliremove(lua_State *L) {
  struct eds_client *cli;
  struct lds_client *lds_cli = checkldsclient(L, 1);
  cli = lds_cli->self;
  eds_service_remove_client(cli->svc, cli);
  return 0;
}

static void init_lds_client(struct eds_client *cli) {
  size_t i;
  struct lds_services *svcs = cli->svc->svc_data.ptr;
  struct lds_client *lds_cli = get_lds_client(cli);
  struct lds_client **dst;
  lua_State *L;

  if (lds_cli != NULL) {
    /* already initialized */
    return;
  }

  /* lookup current svc */
  for (i = 0; i < svcs->nsvcs; i++) {
    if (svcs->svcs + i == cli->svc) {
      break;
    }
  }

  /* no svc found? */
  if (i == svcs->nsvcs) {
    fprintf(stderr, "svc inconsistency: %p not in %p (nsvcs:%zu)\n",
        cli->svc, svcs->svcs, svcs->nsvcs);
    abort();
  }

  L = svcs->L;
  lds_cli = lua_newuserdata(L, sizeof(struct lds_client));
  luaL_setmetatable(L, MTNAME_LDSCLIENT);
  lua_rawgeti(L, LUA_REGISTRYINDEX, (lua_Integer)svcs->tblrefs[i]);
  lua_newtable(L); /* S: cli, {svctbl}, {clitbl} */

  if (lua_getfield(L, -2, "on_readable") == LUA_TFUNCTION) {
    lua_setfield(L, -2, "on_readable");
  } else {
    lua_pop(L, 1);
  }

  if (lua_getfield(L, -2, "on_writable") == LUA_TFUNCTION) {
    lua_setfield(L, -2, "on_writable");
  } else {
    lua_pop(L, 1);
  }

  if (lua_getfield(L, -2, "on_done") == LUA_TFUNCTION) {
    lua_setfield(L, -2, "on_done");
  } else {
    lua_pop(L, 1);
  }

  if (lua_getfield(L, -2, "on_reaped_child") == LUA_TFUNCTION) {
    lua_setfield(L, -2, "on_reaped_child");
  } else {
    lua_pop(L, 1);
  }

  if (lua_getfield(L, -2, "on_eval_error") == LUA_TFUNCTION) {
    lua_setfield(L, -2, "on_eval_error");
  } else {
    lua_pop(L, 1);
  }

  lds_cli->L = L;
  lds_cli->tblref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pop(L, 1); /* pop svctbl, S: cli */
  lds_cli->selfref = luaL_ref(L, LUA_REGISTRYINDEX); /* pop cli */
  lds_cli->self = cli;
  dst = (struct lds_client **)cli->udata;
  *dst = lds_cli;
}

static void handle_pcall_error(struct lds_client *lds_cli) {
  lua_State *L;

  /* assume error obj on TOS */
  L = lds_cli->L;
  lua_rawgeti(L, LUA_REGISTRYINDEX, lds_cli->tblref);
  if (lua_getfield(L, -1, "on_eval_error") == LUA_TFUNCTION) {
    /* S: err cli func */
    lua_rotate(L, -3, 1); /* S: func err clitbl */
    lua_pop(L, 1);
    lua_rawgeti(L, LUA_REGISTRYINDEX, lds_cli->selfref);
    lua_rotate(L, -2, 1); /* S: func cli err */
    lua_pcall(L, 2, 0, 0);
  }
  lua_pop(L, lua_gettop(L));
}

static void dispatch_cli_handler(struct eds_client *cli, int fd,
    const char *name) {
  struct lds_client *lds_cli = get_lds_client(cli);
  int ret;
  int top;
  struct lua_State *L;

  if (lds_cli != NULL) {
    L = lds_cli->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lds_cli->tblref);
    if (lua_getfield(L, -1, name) == LUA_TFUNCTION) {
      lua_rotate(L, lua_gettop(L)-1, 1);
      lua_pop(L, 1);
      lua_rawgeti(L, LUA_REGISTRYINDEX, lds_cli->selfref);
      lua_pushinteger(L, (lua_Integer)fd);
      ret = lua_pcall(L, 2, 0, 0);
      if (ret != LUA_OK) {
        handle_pcall_error(lds_cli);
        eds_service_remove_client(cli->svc, cli);
      }
      /* clear stack (should not be needed, paranoia)  */
      top = lua_gettop(L);
      if (top > 0) {
        lua_pop(L, top);
      }
    } else {
      lua_pop(L, 2);
      eds_service_remove_client(cli->svc, cli);
    }
  }
}

static void on_readable(struct eds_client *cli, int fd) {
  (void)fd;
  dispatch_cli_handler(cli, fd, "on_readable");
}

static void on_writable(struct eds_client *cli, int fd) {
  (void)fd;
  dispatch_cli_handler(cli, fd, "on_writable");
}


static void on_first_readable(struct eds_client *cli, int fd) {
  init_lds_client(cli);
  eds_client_set_on_readable(cli, on_readable, 0);
}

static void on_first_writable(struct eds_client *cli, int fd) {
  init_lds_client(cli);
  eds_client_set_on_writable(cli, on_writable, 0);
}

static void on_done(struct eds_client *cli, int fd) {
  struct lds_client *lds_cli = get_lds_client(cli);
  lua_State *L;
  int ret;

  if (lds_cli != NULL) {
    L = lds_cli->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lds_cli->tblref); /* S: {clitbl} */
    if (lua_getfield(L, -1, "on_done") == LUA_TFUNCTION) { /* S: {clitbl} v */
      lua_rotate(L, lua_gettop(L)-1, 1);
      lua_pop(L, 1);
      lua_rawgeti(L, LUA_REGISTRYINDEX, lds_cli->selfref);
      lua_pushinteger(L, (lua_Integer)fd);
      ret = lua_pcall(L, 2, 0, 0);
      if (ret != LUA_OK) {
        handle_pcall_error(lds_cli);
      }
    } else {
      lua_pop(L, 2);
    }

    luaL_unref(L, LUA_REGISTRYINDEX, lds_cli->tblref);
    luaL_unref(L, LUA_REGISTRYINDEX, lds_cli->selfref);
  }
}

static void on_svc_error(struct eds_service *svc, const char *err) {
  size_t i;
  lua_State *L;
  struct lds_services *svcs = svc->svc_data.ptr;

  for (i = 0; i < svcs->nsvcs; i++) {
    if (svcs->svcs + i == svc) {
      break;
    }
  }

  if (i < svcs->nsvcs) {
    L = svcs->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, svcs->tblrefs[i]);
    if (lua_getfield(L, -1, "on_svc_error") == LUA_TFUNCTION) {
      lua_pushstring(L, err);
      lua_pcall(L, 1, 0, 0);
      lua_pop(L, 1);
    } else {
      lua_pop(L, 2);
    }
  }
}

static void on_reaped_child(struct eds_service *svc, struct eds_client *cli,
      pid_t pid, int status) {
  struct lds_client *lds_cli = get_lds_client(cli);
  struct lua_State *L;
  if (lds_cli != NULL) {
    L = lds_cli->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lds_cli->tblref);
    if (lua_getfield(L, -1, "on_reaped_child") == LUA_TFUNCTION) {
      /* S: {clitbl} func */
      lua_pushinteger(L, (lua_Integer)pid);
      lua_pushinteger(L, (lua_Integer)status);
      lua_pcall(L, 2, 0, 0);
      lua_pop(L, 1);
    } else {
      lua_pop(L, 2);
    }
  }
}

static int l_service(lua_State *L, struct lds_services *svcs, size_t svcpos) {
  int tp;
  struct eds_service *svc;
  const char *cptr;
  lua_Integer li;
  int has_cbs = 0;

  tp = lua_type(L, -1);
  if (tp != LUA_TTABLE) {
    return luaL_error(L, "svc %d: expected table, got %s", (int)svcpos,
        lua_typename(L, tp));
  }

  svc = svcs->svcs + svcpos;

  if (lua_getfield(L, -1, "name") != LUA_TSTRING) {
    return luaL_error(L, "svc %d: missing or invalid name", (int)svcpos);
  }
  cptr = lua_tostring(L, -1);
  svc->name = strdup(cptr);
  if (svc->name == NULL) {
    return luaL_error(L, "memory allocation error");
  }
  lua_pop(L, 1);

  if (lua_getfield(L, -1, "path") != LUA_TSTRING) {
    return luaL_error(L, "svc %d: missing or invalid path", (int)svcpos);
  }
  cptr = lua_tostring(L, -1);
  svc->path = strdup(cptr);
  if (svc->path == NULL) {
    return luaL_error(L, "memory allocation error");
  }
  lua_pop(L, 1);

  if (lua_getfield(L, -1, "nprocs") == LUA_TNUMBER) {
    li = lua_tointeger(L, -1);
    if (li > 0) {
      svc->nprocs = (unsigned int)li;
    }
  }
  lua_pop(L, 1);

  if (lua_getfield(L, -1, "nfds") == LUA_TNUMBER) {
    li = lua_tointeger(L, -1);
    if (li > 0) {
      svc->nfds = (unsigned int)li;
    }
  }
  lua_pop(L, 1);

  if (lua_getfield(L, -1, "tick_slice_us") == LUA_TNUMBER) {
    li = lua_tointeger(L, -1);
    if (li > 0) {
      svc->tick_slice_us = (unsigned int)li;
    }
  }
  lua_pop(L, 1);

  if (lua_getfield(L, -1, "on_readable") == LUA_TFUNCTION) {
    has_cbs = 1;
    svc->actions.on_readable = on_first_readable;
  }
  lua_pop(L, 1);

  if (lua_getfield(L, -1, "on_writable") == LUA_TFUNCTION) {
    has_cbs = 1;
    svc->actions.on_writable = on_first_writable;
  }
  lua_pop(L, 1);

  if (!has_cbs) {
    return luaL_error(L,
        "svc %d: missing on_readable and/or on_writable callback",
        (int)svcpos);
  }

  svc->actions.on_done = on_done;

  if (lua_getfield(L, -1, "on_svc_error") == LUA_TFUNCTION) {
    svc->on_svc_error = on_svc_error;
  }
  lua_pop(L, 1);

  if (lua_getfield(L, -1, "on_reaped_child") == LUA_TFUNCTION) {
    svc->on_reaped_child = on_reaped_child;
  }
  lua_pop(L, 1);

  svc->udata_size = sizeof(struct lds_client);
  svc->svc_data.ptr = svcs;
  svcs->tblrefs[svcpos] = luaL_ref(L, LUA_REGISTRYINDEX); /* pops tbl */
  svcs->L = L;
  return 0;
}

static int l_edsservices(lua_State *L) {
  struct lds_services *svcs;
  size_t nsvcs;
  size_t svclen;
  size_t i;

  luaL_checktype(L, 1, LUA_TTABLE);
  nsvcs = lua_rawlen(L, 1);
  svclen = sizeof(struct lds_services) +
      ((nsvcs+1) * sizeof(struct eds_service)); /* +1 because of sentinel */
  svcs = lua_newuserdata(L, svclen);
  luaL_setmetatable(L, MTNAME_LDSSERVICES);
  memset(svcs, 0, svclen);
  svcs->nsvcs = nsvcs;
  svcs->tblrefs = malloc(sizeof(int) * nsvcs);
  if (svcs->tblrefs == NULL) {
    return luaL_error(L, "unable to allocate service reference table");
  }
  for (i = 0; i < nsvcs; i++) {
    svcs->tblrefs[i] = LUA_REFNIL;
  }

  for (i = 1; i <= nsvcs; i++) {
    lua_rawgeti(L, 1, (lua_Integer)i);
    l_service(L, svcs, i-1);
  }

  return 1;
}

static const struct luaL_Reg services_m[] = {
  {"__gc", l_edsgc},
  {"nsvcs", l_edsnsvcs},
  {"serve", l_edsserve},
  {"serve_single_by_name", l_edsservesinglebyname},
  {NULL, NULL},
};

static const struct luaL_Reg cli_m[] = {
  {"data", l_clidata},
  {"remove", l_cliremove},
  {NULL, NULL},
};

static const struct luaL_Reg eds_f[] = {
  {"services", l_edsservices},
  {NULL, NULL},
};

int luaopen_eds(lua_State *L) {
  /* register service meta-table */
  luaL_newmetatable(L, MTNAME_LDSSERVICES);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, services_m, 0);

  /* register client meta-table */
  luaL_newmetatable(L, MTNAME_LDSCLIENT);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, cli_m, 0);

  /* register eds library functions */
  luaL_newlib(L, eds_f);
  return 1;
}
