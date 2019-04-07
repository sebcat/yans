#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <dlfcn.h>

#include <lib/util/sc2mod.h>
#include <lib/util/io.h>
#include <lib/util/sandbox.h>
#include <lib/util/ylog.h>
#include <lib/util/os.h>

static struct sc2_ctx *ctx_; /* used for child reaping from signal handler */
static int term_; /* set to 1 if SIGTERM, SIGINT, SIGHUP is passed */

#define DAEMON_NAME "sc2"
#define DEFAULT_MAXREQS  64 /* maximum number of concurrent requests */
#define DEFAULT_LIFETIME 5  /* maximum number of seconds for a request */

/* default resource limits */
#define DEFAULT_RLIMIT_VMEM RLIM_INFINITY
#define DEFAULT_RLIMIT_CPU  RLIM_INFINITY

#define SC2_OK        0
#define SC2_EINVAL   -1 /* invalid parameter */
#define SC2_EMEM     -2 /* memory allocation error */
#define SC2_EPOLL    -3 /* poll failure */
#define SC2_EACCEPT  -4 /* accept failure */
#define SC2_EFORK    -5 /* fork failure */
#define SC2_EGETTIME -6 /* clock_gettime failure */
#define SC2_ERRBUF   -7 /* actual error set in errbuf buffer */

/** struct sc2_child flags */
/* indicates that the child has been waited for, and its exit status and
 * completion time is valid */
#define SC2CHLD_WAITED (1 << 0)

/* options specific to sc2 */
struct sc2_opts {
  int maxreqs;     /* Maximum number of concurrent requests */
  time_t lifetime; /* Maximum number of seconds for a request */
  const char *scgi_module; /* path to SCGI module */
  struct rlimit rlim_vmem; /* RLIMIT_VMEM/RLIMIT_AS, in kilobytes */
  struct rlimit rlim_cpu;  /* RLIMIT_CPU, in seconds */
  int sandbox;             /* if non-zero, the child is run in sandbox */

  /* if non-zero, serve the request in the parent process. The process
   * will be exit when the request is served. This is useful for debugging
   * sc2 handlers. */
  int once;

  /* status callbacks */
  void (*on_started)(pid_t);
  void (*on_timedout)(pid_t, struct timespec *);
  void (*on_reaped)(pid_t, int, struct timespec *);
};

/* application options */
struct opts {
  struct sc2_opts sc2;
  const char *basepath;
  uid_t uid;
  gid_t gid;
  int no_daemon;
  int daemon_flags;
  const char *daemon_name;
};

struct sc2_child {
  int flags;
  int status;
  pid_t pid;
  struct timespec started;
  struct timespec completed;
};

struct sc2_ctx {
  struct sc2_opts opts;
  int used;               /* number of currently used request slots */
  struct sc2_child *children;
  char errbuf[128];

  void *so;                            /* SCGI module */
  int (*setup)(struct sc2mod_ctx *);   /* SCGI module setup function */
  int (*handler)(struct sc2mod_ctx *); /* SCGI module request handler */
};

static void set_default_signals() {
  signal(SIGPIPE, SIG_DFL);
  signal(SIGINT, SIG_DFL);
  signal(SIGHUP, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
  signal(SIGCHLD, SIG_DFL);
}

static const char *sc2_strerror(struct sc2_ctx *ctx, int code) {
  switch (code) {
  case SC2_OK:
    return "success";
  case SC2_EINVAL:
    return "invalid parameter";
  case SC2_EMEM:
    return "memory allocation error";
  case SC2_EPOLL:
    return "poll failure";
  case SC2_EACCEPT:
    return "accept failure";
  case SC2_EFORK:
    return "fork failure";
  case SC2_EGETTIME:
    return "clock_gettime failure";
  case SC2_ERRBUF:
    return ctx->errbuf;
  default:
    return "unknown error";
  }
}

static int sc2_init(struct sc2_ctx *ctx, struct sc2_opts *opts) {
  void *so;
  int (*setup)(struct sc2mod_ctx *);
  int (*handler)(struct sc2mod_ctx *);

  assert(ctx != NULL);

  ctx->errbuf[0] = '\0';

  if (opts->maxreqs <= 0) {
    return SC2_EINVAL;
  }

  /* load the SCGI module */
  so = dlopen(opts->scgi_module, RTLD_NOW);
  if (so == NULL) {
    snprintf(ctx->errbuf, sizeof(ctx->errbuf), "%s: %s", opts->scgi_module,
        dlerror());
    return SC2_ERRBUF;
  }

  /* load the SCGI module handler symbol */
  handler = dlsym(so, "sc2_handler");
  if (handler == NULL) {
    snprintf(ctx->errbuf, sizeof(ctx->errbuf), "%s: %s", opts->scgi_module,
        dlerror());
    dlclose(so);
    return SC2_ERRBUF;
  }

  /* load the SCGI module setup symbol, if any (OK to be NULL) */
  setup = dlsym(so, "sc2_setup");

  ctx->children = calloc((size_t)opts->maxreqs, sizeof(struct sc2_child));
  if (ctx->children == NULL) {
    dlclose(so);
    return SC2_EMEM;
  }

  ctx->opts = *opts;
  ctx->used = 0;
  ctx->so = so;
  ctx->setup = setup;
  ctx->handler = handler;
  return SC2_OK;
}

static void sc2_cleanup(struct sc2_ctx *ctx) {
  if (ctx != NULL) {
    /* TODO: kill children, if any */
    if (ctx->so) {
      dlclose(ctx->so);
    }
    free(ctx->children);
    memset(ctx, 0, sizeof(*ctx));
  }
}

static int sc2_serve_incoming(struct sc2_ctx *ctx, int listenfd) {
  int cli;
  int i;
  int ret;
  pid_t pid;
  struct timespec started = {0};
  struct sc2mod_ctx mod = {0};

  do {
    cli = accept(listenfd, NULL, NULL);
  } while (cli < 0 && errno == EINTR);
  if (cli < 0) {
    return SC2_EACCEPT;
  }

  ret = clock_gettime(CLOCK_MONOTONIC, &started);
  if (ret != 0) {
    close(cli);
    return SC2_EGETTIME;
  }

  if (ctx->opts.once == 0) {
    pid = fork();
  } else {
    pid = 0;
  }

  if (pid < 0) {
    return SC2_EFORK;
  } else if (pid == 0) {
    set_default_signals();
    if (ctx->opts.rlim_cpu.rlim_cur != RLIM_INFINITY) {
      setrlimit(RLIMIT_CPU, &ctx->opts.rlim_cpu);
    }
    if (ctx->opts.rlim_vmem.rlim_cur != RLIM_INFINITY) {
      setrlimit(RLIMIT_AS, &ctx->opts.rlim_vmem);
    }
    dup2(cli, STDIN_FILENO);
    dup2(cli, STDOUT_FILENO);
    dup2(cli, STDERR_FILENO);
    close(cli);
    close(listenfd);

    if (ctx->setup) {
      ret = ctx->setup(&mod);
      if (ret != 0) {
        ylog_error("sc2_setup: %s", sc2mod_strerror(&mod));
        exit(abs(ret));
      }
    }

    if (ctx->opts.sandbox) {
      ret = sandbox_enter();
      if (ret != 0) {
        exit(42);
      }
    }

    exit(ctx->handler(&mod));
  }

  close(cli);
  for (i = 0; i < ctx->opts.maxreqs; i++) {
    if (ctx->children[i].pid <= 0) {
      ctx->children[i].flags = 0;
      ctx->children[i].status = 0;
      ctx->children[i].completed.tv_sec = 0;
      ctx->children[i].completed.tv_nsec = 0;
      ctx->children[i].pid = pid;
      ctx->children[i].started = started;
      break;
    }
  }

  ctx->used++;
  if (ctx->opts.on_started) {
    ctx->opts.on_started(pid);
  }

  /* we shouldn't call sc2_serve_incoming w/o any open slots */
  assert(i < ctx->opts.maxreqs);

  return SC2_OK;
}

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


static void sc2_check_procs(struct sc2_ctx *ctx) {
  int i;
  int nleft;
  struct sc2_child *child;
  struct timespec now = {0};
  struct timespec delta = {0};

  clock_gettime(CLOCK_MONOTONIC, &now);
  for (nleft = ctx->used, i = 0; nleft > 0 && i < ctx->opts.maxreqs; i++) {
    child = &ctx->children[i];
    if (child->pid > 0 && (child->flags & SC2CHLD_WAITED) == 0) {
      delta = timespec_delta(child->started, now);
      if (delta.tv_sec >= ctx->opts.lifetime) {
        /* TODO: SIGTERM grace period? */
        kill(child->pid, SIGKILL);
        if (ctx->opts.on_timedout) {
          ctx->opts.on_timedout(child->pid, &delta);
        }
      }
    }
  }
}

static void sc2_reset_reaped(struct sc2_ctx *ctx) {
  int i;
  struct sc2_child *child;

  for (i = 0; i < ctx->opts.maxreqs; i++) {
    child = &ctx->children[i];
    if (child->flags & SC2CHLD_WAITED) {
      if (ctx->opts.on_reaped) {
        struct timespec delta;
        delta = timespec_delta(child->started, child->completed);
        ctx->opts.on_reaped(child->pid, child->status, &delta);
      }
      ctx->used--;
      child->pid = 0;
      child->flags &= ~(SC2CHLD_WAITED);
    }
  }
}

static void sc2_reap_children(struct sc2_ctx *ctx) {
  pid_t pid;
  int status;
  int i;
  struct timespec now = {0};
  struct sc2_child *child;

  /* NB: this function needs to be async-signal safe */
  clock_gettime(CLOCK_MONOTONIC, &now);
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    for (i = 0; i < ctx->opts.maxreqs; i++) {
      child = &ctx->children[i];
      if (child->pid == pid) {
        child->flags |= SC2CHLD_WAITED;
        child->completed = now;
        child->status = status;
        break;
      }
    }
  }
}

static int sc2_serve(struct sc2_ctx *ctx, int listenfd) {
  struct pollfd pfd;
  int ret;

  pfd.fd = listenfd;
  pfd.events = POLLIN;
  while (1) {
    sc2_check_procs(ctx);
    sc2_reset_reaped(ctx);

    /* break the main loop on requested termination */
    if (term_) {
      break;
    }

    /* If we'e at capacity, do not accept any more incoming connections.
     * If a child is done, sleep will return early and waitpid will handle
     * the child at the top of the loop */
    if (ctx->used >= ctx->opts.maxreqs) {
      sleep(1);
      continue;
    }

    /* timeout 1000 because we need to check for process timeouts, but only
     * if we have waiting processes */
    ret = poll(&pfd, 1, ctx->used > 0 ? 1000 : -1);
    if (ret < 0) {
      if (errno == EINTR) {
        continue;
      } else {
        return SC2_EPOLL;
      }
    } else if (ret == 0) {
      continue;
    }

    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
      return SC2_EPOLL;
    }

    if (pfd.revents & POLLIN) {
      ret = sc2_serve_incoming(ctx, listenfd);
      if (ret != SC2_OK) {
        return ret;
      }
    }
  }


  return SC2_OK;
}

static void usage() {
  fprintf(stderr,
      "usage:\n"
      "  " DAEMON_NAME " -u <user> -g <group> -b <basepath> <scgi-module>\n"
      "  " DAEMON_NAME " -n -b <basepath> <scgi-module>\n"
      "  " DAEMON_NAME " -h\n"
      "\n"
      "options:\n"
      "  -u, --user:      daemon user\n"
      "  -g, --group:     daemon group\n"
      "  -b, --basepath:  working directory basepath\n"
      "  -n, --no-daemon: do not daemonize\n"
      "  -m, --maxreqs:   number of max concurrent requests (%d)\n"
      "  -l, --lifetime:  maximum number of seconds for a request (%d)\n"
      "  -0, --no-chroot: do not chroot daemon\n"
      "  -t, --cpu-rlim:  max CPU resource limit, in seconds\n"
      "  -v, --vmem-rlim: max virtual memory, in kbytes\n"
      "  -1, --once:      serve only once request and serve in parent\n"
      "  -N, --name:      set daemon name (" DAEMON_NAME ")\n"
      "  -h, --help:      this text\n",
      DEFAULT_MAXREQS, DEFAULT_LIFETIME);
  exit(EXIT_FAILURE);
}

static void log_on_timedout(pid_t pid, struct timespec *t) {
  ylog_error("timeout duration:%ld.%.9lds pid:%ld",
      (long)t->tv_sec, (long)t->tv_nsec, (long)pid);
}

static void log_on_reaped(pid_t pid, int status, struct timespec *t) {
  int exitcode;

  if (WIFEXITED(status)) {
    exitcode = WEXITSTATUS(status);
    if (exitcode == 0) {
      ylog_info("served duration:%ld.%.9lds pid:%ld exit:0",
          (long)t->tv_sec, (long)t->tv_nsec, (long)pid);
    } else {
      ylog_error("served duration:%ld.%.9lds pid:%ld exit:%d",
          (long)t->tv_sec, (long)t->tv_nsec, (long)pid, exitcode);
    }
  } else if (WIFSIGNALED(status)) {
    ylog_error("served duration:%ld.%.9lds pid:%ld signal:%d%s",
        (long)t->tv_sec, (long)t->tv_nsec, (long)pid, WTERMSIG(status),
        WCOREDUMP(status) ? " (core dumped)" : "");
  }
}

static long long_or_die(const char *str, int opt) {
  long ret;
  char *ptr = NULL;

  ret = strtol(str, &ptr, 10);
  if (*ptr != '\0') {
    fprintf(stderr, "-%c: invalid number\n", opt);
    exit(EXIT_FAILURE);
  }

  return ret;
}

int is_valid_daemon_name(const char *name) {
  char ch;

  if (!name || !*name) {
    return 0;
  }

  while ((ch = *name++) != '\0') {
    if ((ch < 'a' || ch > 'z') &&
        (ch < '0' || ch > '9') &&
        ch != '-') {
      return 0;
    }
  }

  return 1;
}

static void parse_args_or_die(struct opts *opts, int argc, char **argv) {
  int ch;
  os_t os;
  static const char *optstr = "u:g:b:nl:m:0t:v:X1N:h";
  static struct option longopts[] = {
    {"user", required_argument, NULL, 'u'},
    {"group", required_argument, NULL, 'g'},
    {"basepath", required_argument, NULL, 'b'},
    {"maxreqs", required_argument, NULL, 'n'},
    {"lifetime", required_argument, NULL, 'l'},
    {"no-daemon", no_argument, NULL, 'n'},
    {"no-chroot", no_argument, NULL, '0'},
    {"cpu-rlim", required_argument, NULL, 't'},
    {"vmem-rlim", required_argument, NULL, 'v'},
    {"no-sandbox", no_argument, NULL, 'X'},
    {"once", no_argument, NULL, '1'},
    {"name", required_argument, NULL, 'N'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
  };

  /* init default values */
  opts->basepath = NULL;
  opts->uid = 0;
  opts->gid = 0;
  opts->no_daemon = 0;
  opts->daemon_flags = 0;
  opts->daemon_name = DAEMON_NAME;
  opts->sc2.sandbox = 1;
  opts->sc2.maxreqs = DEFAULT_MAXREQS;
  opts->sc2.lifetime = DEFAULT_LIFETIME;
  opts->sc2.rlim_vmem.rlim_cur = DEFAULT_RLIMIT_VMEM;
  opts->sc2.rlim_vmem.rlim_max = DEFAULT_RLIMIT_VMEM;
  opts->sc2.rlim_cpu.rlim_cur = DEFAULT_RLIMIT_CPU;
  opts->sc2.rlim_cpu.rlim_max = DEFAULT_RLIMIT_CPU;
  opts->sc2.once = 0;
  opts->sc2.on_reaped = log_on_reaped;
  opts->sc2.on_timedout = log_on_timedout;

  while ((ch = getopt_long(argc, argv, optstr, longopts, NULL)) != -1) {
    switch (ch) {
      case 'u':
        if (os_getuid(&os, optarg, &opts->uid) != OS_OK) {
          fprintf(stderr, "%s\n", os_strerror(&os));
          usage();
        }
        break;
      case 'g':
        if(os_getgid(&os, optarg, &opts->gid) != OS_OK) {
          fprintf(stderr, "%s\n", os_strerror(&os));
          usage();
        }
        break;
      case 'b':
        opts->basepath = optarg;
        break;
      case 'm':
        opts->sc2.maxreqs = (int)long_or_die(optarg, 'm');
        break;
      case 'n':
        opts->no_daemon = 1;
        break;
      case 'l':
        opts->sc2.lifetime = (time_t)long_or_die(optarg, 'l');
        break;
      case '0':
        opts->daemon_flags |= DAEMONOPT_NOCHROOT;
        break;
      case 't':
        opts->sc2.rlim_cpu.rlim_cur = long_or_die(optarg, 't');
        opts->sc2.rlim_cpu.rlim_max = opts->sc2.rlim_cpu.rlim_cur;
        break;
      case 'v':
        opts->sc2.rlim_vmem.rlim_cur = long_or_die(optarg, 'v');
        opts->sc2.rlim_vmem.rlim_max = opts->sc2.rlim_vmem.rlim_cur;
        break;
      case 'X':
        opts->sc2.sandbox = 0;
        break;
      case '1':
        opts->sc2.once = 1;
        break;
      case 'N':
        opts->daemon_name = optarg;
        break;
      case 'h':
      default:
        usage();
    }
  }

  /* sanity check opts */
  if (opts->no_daemon == 0 && (opts->gid == 0 || opts->uid == 0)) {
    fprintf(stderr, "daemon must run as unprivileged user:group\n");
    usage();
  } else if (opts->sc2.maxreqs < 1) {
    fprintf(stderr, "maxreqs must be greater than zero\n");
    usage();
  } else if (opts->sc2.maxreqs > 1024) {
    fprintf(stderr, "maxreqs can not be higher than 1024\n");
    usage();
  } else if (opts->sc2.lifetime < 1 || opts->sc2.lifetime > 300) {
    fprintf(stderr, "invalid lifetime\n");
    usage();
  } else if (opts->basepath == NULL) {
    fprintf(stderr, "missing basepath\n");
    usage();
  } else if (!is_valid_daemon_name(opts->daemon_name)) {
    fprintf(stderr, "invalid daemon name\n");
    usage();
  }

  opts->basepath = os_cleanpath((char*)opts->basepath);
  if (opts->basepath[0] != '/') {
    fprintf(stderr, "basepath must be an absolute path\n");
    usage();
  }

  argc -= optind;
  argv += optind;
  if (argc <= 0) {
    fprintf(stderr, "missing path to SCGI module\n");
    usage();
  }

  /* cleanup the SCGI module path */
  opts->sc2.scgi_module = os_cleanpath(argv[0]);
}

static void on_sigchld(int signal) {
  int saved_errno;

  if (signal == SIGCHLD) {
    saved_errno = errno;
    sc2_reap_children(ctx_);
    errno = saved_errno;
  }
}

static void on_term(int signal) {
  term_ = 1;
}

static int serve(struct sc2_ctx *ctx, int listenfd) {
  int ret;
  struct sigaction sa;

  /* This is a bit ugly, but so are signals... */
  ctx_ = ctx;
  sa.sa_handler = &on_sigchld;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  sigaction(SIGCHLD, &sa, 0);
  sa.sa_handler = &on_term;
  sigaction(SIGTERM, &sa, 0);
  sigaction(SIGHUP, &sa, 0);
  sigaction(SIGINT, &sa, 0);
  signal(SIGPIPE, SIG_IGN);
  ret = sc2_serve(ctx, listenfd);
  set_default_signals();
  ctx_ = NULL;

  return ret;
}

int main(int argc, char *argv[]) {
  io_t io;
  os_t os;
  struct sc2_ctx ctx = {{0}};
  struct opts opts = {{0}};
  struct os_daemon_opts daemon_opts = {0};
  int status = EXIT_FAILURE;
  char *sockpath;
  int ret;

  parse_args_or_die(&opts, argc, argv);

  /* build sockpath */
  ret = asprintf(&sockpath, "%s.sock", opts.daemon_name);
  if (ret < 0) {
    fprintf(stderr, "%s.sock: failed to allocate string\n",
        opts.daemon_name);
    goto end;
  }

  /* init sc2 before chroot and daemonization so that the .so can live
   * outside of the chroot */
  ret = sc2_init(&ctx, &opts.sc2);
  if (ret != SC2_OK) {
    fprintf(stderr, "sc2_init: %s\n", sc2_strerror(&ctx, ret));
    goto free_sockpath;
  }

  if (opts.no_daemon) {
    ylog_init(opts.daemon_name, YLOG_STDERR);
    if (chdir(opts.basepath) < 0) {
      ylog_error("chdir %s: %s", opts.basepath, strerror(errno));
      goto sc2_cleanup;
    }
  } else {
    ylog_init(opts.daemon_name, YLOG_SYSLOG);
    daemon_opts.flags = opts.daemon_flags;
    daemon_opts.name = opts.daemon_name;
    daemon_opts.path = opts.basepath;
    daemon_opts.uid = opts.uid;
    daemon_opts.gid = opts.gid;
    daemon_opts.nagroups = 0;
    if (os_daemonize(&os, &daemon_opts) != OS_OK) {
      ylog_error("%s", os_strerror(&os));
      goto sc2_cleanup;
    }
  }

  ret = io_listen_unix(&io, sockpath);
  if (ret != IO_OK) {
    ylog_error("%s: %s", sockpath, io_strerror(&io));
    goto sc2_cleanup;
  }

  ylog_info("Starting %s", opts.daemon_name);
  ret = serve(&ctx, IO_FILENO(&io));
  if (ret != SC2_OK) {
    ylog_error("sc2_serve: %s", sc2_strerror(&ctx, ret));
    ylog_error("failed to serve %s", opts.daemon_name);
  } else {
    status = EXIT_SUCCESS;
  }

  if (!opts.no_daemon) {
    ret = os_daemon_remove_pidfile(&os, &daemon_opts);
    if (ret != OS_OK) {
      ylog_error("unable to remove pidfile: %s", os_strerror(&os));
      status = EXIT_FAILURE;
    }
  }

  io_close(&io);
sc2_cleanup:
  sc2_cleanup(&ctx);
free_sockpath:
  free(sockpath);
end:
  return status;
}
