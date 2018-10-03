#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <lib/util/io.h>
#include <lib/lua/sc2.h>

/* default values for serve parameters */
#define DEFAULT_MAXREQS     64
#define DEFAULT_LIFETIME    20
#define DEFAULT_RLIMIT_VMEM RLIM_INFINITY
#define DEFAULT_RLIMIT_CPU  RLIM_INFINITY

/* default serve parameter names */
#define PARAM_PATH        "path"
#define PARAM_MAXREQS     "maxreqs"
#define PARAM_LIFETIME    "lifetime"
#define PARAM_RLIMIT_VMEM "rlimit_vmem"
#define PARAM_RLIMIT_CPU  "rlimit_cpu"
#define PARAM_SERVEFUNC   "servefunc"
#define PARAM_DONEFUNC    "donefunc"
#define PARAM_ERRFUNC     "errfunc"

/** struct sc2_child flags */
/* indicates that the child has been waited for, and its exit status and
 * completion time is valid */
#define SC2CHLD_WAITED (1 << 0)

struct sc2_child {
  int flags;
  int status;
  pid_t pid;
  struct timespec started;
  struct timespec completed;
};

static int max_children_;           /* maximum number of concurrent children */
static int active_children_;        /* number of currently active children */
static struct sc2_child *children_; /* child states */
static int term_;                   /* set to true by sigh on teardown */
static int cmdfd_;                  /* socket for incoming connections */
static int lifetime_;               /* lifetime of a child */
static struct rlimit rlim_cpu_;     /* child CPU rlimit */
static struct rlimit rlim_vmem_;    /* child VMEM/AS rlimit */

struct timespec timespec_delta(struct timespec start, struct timespec end) {
  struct timespec temp;

  if ((end.tv_nsec - start.tv_nsec) < 0) {
    temp.tv_sec = end.tv_sec - start.tv_sec - 1;
    temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
  } else {
    temp.tv_sec = end.tv_sec - start.tv_sec;
    temp.tv_nsec = end.tv_nsec - start.tv_nsec;
  }

  return temp;
}

static void sc2_reap_children() {
  pid_t pid;
  int status;
  int i;
  struct timespec now = {0};
  struct sc2_child *child;

  /* NB: this function needs to be async-signal safe */
  clock_gettime(CLOCK_MONOTONIC, &now);
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    for (i = 0; i < max_children_; i++) {
      child = &children_[i];
      if (child->pid == pid) {
        child->flags |= SC2CHLD_WAITED;
        child->completed = now;
        child->status = status;
        break;
      }
    }
  }
}

static void on_sigchld(int signal) {
  int saved_errno;

  if (signal == SIGCHLD) {
    saved_errno = errno;
    sc2_reap_children();
    errno = saved_errno;
  }
}

static void on_term(int signal) {
  term_ = 1;
}

static void sc2_check_procs() {
  int i;
  int nleft;
  struct sc2_child *child;
  struct timespec now = {0};
  struct timespec delta = {0};

  clock_gettime(CLOCK_MONOTONIC, &now);
  for (nleft = active_children_, i = 0; nleft > 0 && i < max_children_; i++) {
    child = &children_[i];
    if (child->pid > 0 && (child->flags & SC2CHLD_WAITED) == 0) {
      delta = timespec_delta(child->started, now);
      if (delta.tv_sec >= lifetime_) {
        /* TODO: SIGTERM grace period? */
        kill(child->pid, SIGKILL);
      }
    }
  }
}

static void sc2_reset_reaped(lua_State *L) {
  int i;
  struct sc2_child *child;
  double duration;
  struct timespec delta;

  for (i = 0; i < max_children_; i++) {
    child = &children_[i];
    if (child->flags & SC2CHLD_WAITED) {
      delta = timespec_delta(child->started, child->completed);
      duration = (double)delta.tv_sec + (double)delta.tv_nsec / 1e9;
      lua_getfield(L, -1, PARAM_DONEFUNC);
      lua_pushinteger(L, child->pid);
      lua_pushnumber(L, duration);
      if (WIFEXITED(child->status)) {
        lua_pushliteral(L, "exit");
        lua_pushinteger(L, WEXITSTATUS(child->status));
      } else if (WIFSIGNALED(child->status)) {
        lua_pushliteral(L, "signal");
        lua_pushinteger(L, WTERMSIG(child->status));
      } else {
        lua_pushnil(L);
        lua_pushnil(L);
      }

      lua_call(L, 4, 0);
      active_children_--;
      child->pid = 0;
      child->flags &= ~(SC2CHLD_WAITED);
    }
  }
}

static int l_sc2_serve_incoming(lua_State *L) {
  int cli;
  int i;
  int ret;
  pid_t pid;
  struct timespec started = {0};
  struct sc2_child *child;

  do {
    cli = accept(cmdfd_, NULL, NULL);
  } while (cli < 0 && errno == EINTR);
  if (cli < 0) {
    return luaL_error(L, "accept: %s", strerror(errno));
  }

  ret = clock_gettime(CLOCK_MONOTONIC, &started);
  if (ret != 0) {
    close(cli);
    return luaL_error(L, "clock_gettime: %s", strerror(errno));
  }

  pid = fork();
  if (pid < 0) {
    return luaL_error(L, "fork: %s", strerror(errno));
  } else if (pid == 0) {
    if (rlim_cpu_.rlim_max != RLIM_INFINITY) {
      setrlimit(RLIMIT_CPU, &rlim_cpu_);
    }
    if (rlim_vmem_.rlim_max != RLIM_INFINITY) {
      setrlimit(RLIMIT_AS, &rlim_vmem_);
    }
    dup2(cli, STDIN_FILENO);
    dup2(cli, STDOUT_FILENO);
    dup2(cli, STDERR_FILENO);
    close(cli);
    close(cmdfd_);
    lua_getfield(L, -1, PARAM_SERVEFUNC);
    ret = lua_pcall(L, 0, 0, 0);
    if (ret != LUA_OK) {
      /* servefunc failed, call errfunc with the error object and exit */
      lua_getfield(L, -2, PARAM_ERRFUNC); /* o err -- o err func */
      lua_pushvalue(L, -2);               /* o err func -- o err func err */
      lua_pcall(L, 1, 0, 0);
      exit(1);
    }
    exit(0);
  }

  close(cli);
  for (i = 0; i < max_children_; i++) {
    child = &children_[i];
    if (child->pid <= 0) {
      child->pid = pid;
      child->started = started;
      break;
    }
  }
  /* we shouldn't call l_sc2_serve_incoming w/o any open slots */
  assert(i < max_children_);


  active_children_++;

  return 0;
}

static int l_sc2_pserve(lua_State *L) {
  struct pollfd pfd;
  int ret;

  pfd.fd = cmdfd_;
  pfd.events = POLLIN;
  while (1) {
    sc2_check_procs();
    sc2_reset_reaped(L);

    /* break the main loop on requested termination */
    if (term_) {
      break;
    }

    /* If we'e at capacity, do not accept any more incoming connections.
     * If a child is done, sleep will return early and waitpid will handle
     * the child at the top of the loop */
    if (active_children_ >= max_children_) {
      sleep(1);
      continue;
    }

    /* timeout 1000 because we need to check for process timeouts, but only if
     * we have waiting processes */
    ret = poll(&pfd, 1, active_children_ > 0 ? 1000 : -1);
    if (ret < 0) {
      if (errno == EINTR) {
        continue;
      } else {
        return luaL_error(L, "poll: %s", strerror(errno));
      }
    } else if (ret == 0) {
      continue;
    }

    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
      return luaL_error(L, "poll: fd error");
    }

    if (pfd.revents & POLLIN) {
      l_sc2_serve_incoming(L);
    }
  }

  return 0;
}

static void sc2arg_optint(lua_State *L, int index, const char *name,
    lua_Integer dfl) {
  int t;

  t = lua_getfield(L, index, name);
  if (t != LUA_TNUMBER) {
    lua_pushinteger(L, dfl);
    lua_setfield(L, index, name);
  }
  lua_pop(L, 1);
}

static void sc2arg_required(lua_State *L, int index, const char *name,
    int typ) {
  int t;

  t = lua_getfield(L, 1, name);
  if (t != typ) {
    luaL_error(L, "missing '%s'", name);
  }
  lua_pop(L, 1);
}

/*
 * TOS: table with the following values:
 *   - path: required, unix socket path
 *   - maxreqs: optional, maximum number of concurrent requests. Defaults to
 *              DEFAULT_MAXREQS
 *   - lifetime: optional, maximum number of seconds to serve a request.
 *               Defaults to DEFAULT_LIFETIME
 *   - rlimit_vmem: optional, child RLIMIT_VMEM/RLIMIT_AS, in kbytes. Defaults
 *                  to DEFAULT_RLIMIT_VMEM
 *   - rlimit_cpu: optional, child RLIMIT_CPU, in seconds. Defaults to
 *                 DEFAULT_RLIMIT_CPU
 *   - servefunc: required, fn(). Called in child for each accepted client.
 *   - donefunc: required, fn(pid,duration,cause,code). Called in parent on
 *               successful/failed request
 *   - errfunc: required, fn(err). Called in child on Lua error in pcall to
 *              servefunc.
 */
static void validate_sc2_opts(lua_State *L) {
  size_t i;
  static const struct {
    const char *name;
    int t;
  } reqfields[] = {
    {PARAM_PATH, LUA_TSTRING},
    {PARAM_SERVEFUNC, LUA_TFUNCTION},
    {PARAM_DONEFUNC, LUA_TFUNCTION},
    {PARAM_ERRFUNC, LUA_TFUNCTION},
    {NULL, 0},
  };
  static const struct {
    const char *name;
    lua_Integer value;
  } dflints[] = {
    {PARAM_MAXREQS, DEFAULT_MAXREQS},
    {PARAM_LIFETIME, DEFAULT_LIFETIME},
    {PARAM_RLIMIT_VMEM, DEFAULT_RLIMIT_VMEM},
    {PARAM_RLIMIT_CPU, DEFAULT_RLIMIT_CPU},
    {NULL, 0},
  };

  /* set optional numerical arguments */
  for (i = 0; dflints[i].name != NULL; i++) {
    sc2arg_optint(L, 1, dflints[i].name, dflints[i].value);
  }

  /* check required arguments */
  for (i = 0; reqfields[i].name != NULL; i++) {
    sc2arg_required(L, 1, reqfields[i].name, reqfields[i].t);
  }
}

static int l_sc2_serve(lua_State *L) {
  io_t io;
  int ret;
  struct sigaction sa;
  const char *cptr;

  /* if the child states are allocated, we're already running - or we're
   * screwed anyway */
  if (children_ != NULL) {
    return luaL_error(L, "sc2: there can only be one!");
  }

  /* validate the options passed to us, and set default values */
  validate_sc2_opts(L);

  /* reset params not inited below */
  term_ = 0;
  active_children_ = 0;

  /* get the number of maximum concurrent children */
  lua_getfield(L, 1, PARAM_MAXREQS);
  ret = lua_tonumber(L, -1);
  lua_pop(L, 1);
  if (ret <= 0) {
    return luaL_error(L, "invalid '" PARAM_MAXREQS "' value");
  }
  max_children_ = ret;

  /* get the lifetime (in seconds) of a child */
  lua_getfield(L, 1, PARAM_LIFETIME);
  ret = lua_tonumber(L, -1);
  lua_pop(L, 1);
  if (ret <= 0) {
    return luaL_error(L, "invalid '" PARAM_MAXREQS "' value");
  }
  lifetime_ = ret;

  /* get the CPU rlimit */
  lua_getfield(L, 1, PARAM_RLIMIT_CPU);
  rlim_cpu_.rlim_cur = rlim_cpu_.rlim_max = lua_tointeger(L, -1);
  lua_pop(L, 1);

  /* get the VMEM/AS rlimit */
  lua_getfield(L, 1, PARAM_RLIMIT_VMEM);
  rlim_vmem_.rlim_cur = rlim_vmem_.rlim_max = lua_tointeger(L, -1);
  lua_pop(L, 1);

  /* setup initial child states */
  children_ = calloc(max_children_, sizeof(struct sc2_child));
  if (children_ == NULL) {
    return luaL_error(L, "failed to allocate child states");
  }

  /* open the command socket */
  lua_getfield(L, 1, PARAM_PATH);
  cptr = lua_tostring(L, -1);
  ret = io_listen_unix(&io, cptr);
  lua_pop(L, 1);
  if (ret != IO_OK) {
    free(children_);
    children_ = NULL;
    return luaL_error(L, "%s: %s", cptr, io_strerror(&io));
  }
  cmdfd_ = IO_FILENO(&io);

  /* setup the signal handlers */
  sa.sa_handler = &on_sigchld;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  sigaction(SIGCHLD, &sa, 0);
  sa.sa_handler = &on_term;
  sigaction(SIGTERM, &sa, 0);
  sigaction(SIGHUP, &sa, 0);
  sigaction(SIGINT, &sa, 0);

  /* pcall pserve (much protection, such wow). Check return value later */
  lua_pushcfunction(L, l_sc2_pserve);
  lua_pushvalue(L, -2);
  ret = lua_pcall(L, 1, 0, 0);

  /* reset the signal handlers */
  /* XXX: We could have other signal handlers for these registered prior to
   *      calling this serve function, other than SIG_DFL. */
  signal(SIGCHLD, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
  signal(SIGHUP, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  /* close the listening socket */
  io_close(&io);

  /* deallocate the child states */
  /* TODO: Kill all children still alive */
  free(children_);
  children_ = NULL;

  /* Check the return value of lua_pcall on pserve. If pserve failed, we want
   * to pass the error object up the stack to the caller. */
  if (ret != LUA_OK) {
    lua_error(L);
  }

  return 0;
}

static const struct luaL_Reg sc2_f[] = {
  {"serve", l_sc2_serve},
  {NULL, NULL},
};

int luaopen_sc2(lua_State *L) {
  luaL_newlib(L, sc2_f);
  return 1;
}
