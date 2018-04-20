#include <sys/types.h>
#include <sys/socket.h>
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

#include <lib/util/io.h>
#include <lib/util/ylog.h>
#include <lib/util/os.h>

extern char **environ;

#define DAEMON_NAME "sc2"
#define DEFAULT_MAXREQS  64 /* maximum number of concurrent requests */
#define DEFAULT_LIFETIME 20 /* maximum number of seconds for a request */

#define SC2_OK 0
#define SC2_EINVAL   -1 /* invalid parameter */
#define SC2_EMEM     -2 /* memory allocation error */
#define SC2_EPOLL    -3 /* poll failure */
#define SC2_EACCEPT  -4 /* accept failure */
#define SC2_EFORK    -5 /* fork failure */
#define SC2_EGETTIME -6 /* clock_gettime failure */

#define SC2_LISTENFD 0 /* index for listening fd in SC2 vectors */

struct sc2_opts {
  int maxreqs;
  char *cgipath;
  time_t lifetime;

  /* status callbacks */
  void (*on_started)(pid_t);
  void (*on_timedout)(pid_t, struct timespec *);
  void (*on_reaped)(pid_t, int, struct timespec *);
};

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

struct sc2_ctx {
  struct sc2_opts opts;
  int listenfd;
  int used;
  struct sc2_child *children;
};

static const char *sc2_strerror(int code) {
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
  default:
    return "unknown error";
  }
}

static int sc2_init(struct sc2_ctx *ctx, struct sc2_opts *opts, int listenfd) {
  assert(ctx != NULL);

  if (opts->maxreqs <= 0) {
    return SC2_EINVAL;
  }

  ctx->children = calloc((size_t)opts->maxreqs, sizeof(struct sc2_child));
  if (ctx->children == NULL) {
    return SC2_EMEM;
  }

  ctx->opts = *opts;
  ctx->used = 0;
  ctx->listenfd = listenfd;
  return SC2_OK;
}

static void sc2_cleanup(struct sc2_ctx *ctx) {
  if (ctx != NULL) {
    /* TODO: wait for children, if any */
    free(ctx->children);
    memset(ctx, 0, sizeof(*ctx));
  }
}

static int sc2_serve_incoming(struct sc2_ctx *ctx) {
  int cli;
  int i;
  int ret;
  pid_t pid;
  struct timespec started = {0};

  do {
    cli = accept(ctx->listenfd, NULL, NULL);
  } while (cli < 0 && errno == EINTR);
  if (cli < 0) {
    return SC2_EACCEPT;
  }

  ret = clock_gettime(CLOCK_MONOTONIC, &started);
  if (ret != 0) {
    close(cli);
    return SC2_EGETTIME;
  }

  pid = fork();
  if (pid < 0) {
    return SC2_EFORK;
  } else if (pid == 0) {
    char *argv[] = {ctx->opts.cgipath, NULL};
    signal(SIGCHLD, SIG_DFL);
    dup2(cli, STDIN_FILENO);
    dup2(cli, STDOUT_FILENO);
    dup2(cli, STDERR_FILENO);
    close(cli);
    close(ctx->listenfd);
    execve(ctx->opts.cgipath, argv, environ);
    exit(1);
  }

  close(cli);
  for (i = 0; i < ctx->opts.maxreqs; i++) {
    if (ctx->children[i].pid <= 0) {
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

static void sc2_cleanup_reaped(struct sc2_ctx *ctx) {
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

static int sc2_serve(struct sc2_ctx *ctx) {
  struct pollfd pfd;
  int ret;

  pfd.fd = ctx->listenfd;
  pfd.events = POLLIN;
  while (1) {
    sc2_check_procs(ctx);
    sc2_cleanup_reaped(ctx);

    /* If we'e at capacity, do not accept any more incoming connections.
     * If a child is done, sleep will return early and waitpid will handle
     * the child at the top of the loop */
    if (ctx->used >= ctx->opts.maxreqs) {
      sleep(1);
      continue;
    }

    /* timeout 1000 because we need to check for process timeouts, but only if
     * we have waiting processes */
    ret = poll(&pfd, 1, ctx->used > 0 ? 1000 : INFTIM);
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
      ret = sc2_serve_incoming(ctx);
      if (ret != SC2_OK) {
        return ret;
      }
    }
  }


  return 0;
}

struct opts {
  struct sc2_opts sc2;
  const char *basepath;
  uid_t uid;
  gid_t gid;
  int no_daemon;
  int daemon_flags;
};

static void usage() {
  fprintf(stderr,
      "usage:\n"
      "  " DAEMON_NAME " -u <user> -g <group> -b <basepath> <scgi-binary>\n"
      "  " DAEMON_NAME " -n -b <basepath> <cgi-root>\n"
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
      "  -h, --help:      this text\n",
      DEFAULT_MAXREQS, DEFAULT_LIFETIME);
  exit(EXIT_FAILURE);
}

static void log_on_started(pid_t pid) {
  ylog_info("incoming request: PID:%ld", (long)pid);
}

static void log_on_timedout(pid_t pid, struct timespec *t) {
  ylog_error("lifetime exceeded: PID:%ld duration:%ld.%.9lds", (long)pid,
      (long)t->tv_sec, (long)t->tv_nsec);
}

static void log_on_reaped(pid_t pid, int status, struct timespec *t) {
  int exitcode;

  if (WIFEXITED(status)) {
    exitcode = WEXITSTATUS(status);
    if (exitcode == 0) {
      ylog_info("request exited: PID:%ld duration:%ld.%.9lds exit:0",
          (long)pid, (long)t->tv_sec, (long)t->tv_nsec);
    } else {
      ylog_error("request exited: PID:%ld duration:%ld.%.9lds exit:%d",
          (long)pid,(long)t->tv_sec, (long)t->tv_nsec, exitcode);
    }
  } else if (WIFSIGNALED(status)) {
    ylog_error("request exited: PID:%ld duration:%ld.%.9lds signal:%d%s",
        (long)pid, (long)t->tv_sec, (long)t->tv_nsec, WTERMSIG(status),
        WCOREDUMP(status) ? " (core dumped)" : "");
  }
}

static void parse_args_or_die(struct opts *opts, int argc, char **argv) {
  int ch;
  os_t os;
  static const char *optstr = "u:g:b:nl:m:0h";
  static struct option longopts[] = {
    {"user", required_argument, NULL, 'u'},
    {"group", required_argument, NULL, 'g'},
    {"basepath", required_argument, NULL, 'b'},
    {"maxreqs", required_argument, NULL, 'n'},
    {"lifetime", required_argument, NULL, 'l'},
    {"no-daemon", no_argument, NULL, 'n'},
    {"no-chroot", no_argument, NULL, '0'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
  };

  /* init default values */
  opts->basepath = NULL;
  opts->uid = 0;
  opts->gid = 0;
  opts->no_daemon = 0;
  opts->daemon_flags = 0;
  opts->sc2.maxreqs = DEFAULT_MAXREQS;
  opts->sc2.lifetime = DEFAULT_LIFETIME;
  opts->sc2.on_reaped = log_on_reaped;
  opts->sc2.on_timedout = log_on_timedout;
  opts->sc2.on_started = log_on_started;

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
        opts->sc2.maxreqs = (int)strtol(optarg, NULL, 10);
        break;
      case 'n':
        opts->no_daemon = 1;
        break;
      case 'l':
        opts->sc2.lifetime = (time_t)strtol(optarg, NULL, 10);
        break;
      case '0':
        opts->daemon_flags |= DAEMONOPT_NOCHROOT;
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
  }

  if (opts->basepath == NULL) {
    fprintf(stderr, "missing basepath\n");
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
    fprintf(stderr, "missing SCGI binary\n");
    usage();
  }

  /* setup and validate cgipath */
  opts->sc2.cgipath = os_cleanpath(argv[0]);
  if (*opts->sc2.cgipath != '/') {
    fprintf(stderr, "SCGI binary must be an absolute path\n");
    usage();
  } else if (!os_isexec(opts->sc2.cgipath)) {
    fprintf(stderr, "SCGI binary is not executable\n");
    usage();
  }
}

static int open_listenfd(const char *path) {
  io_t io;
  int ret;

  ret = io_listen_unix(&io, path);
  if (ret != IO_OK) {
    ylog_error("%s: %s", path, io_strerror(&io));
    return -1;
  }

  return IO_FILENO(&io);
}

static struct sc2_ctx *ctx_;

static void on_sigchld(int signal) {
  int saved_errno = errno;
  if (signal != SIGCHLD) {
    return;
  }

  sc2_reap_children(ctx_);
  errno = saved_errno;
}

static int serve(struct opts *opts) {
  struct sigaction sa;
  struct sc2_ctx ctx = {{0}};
  int ret;
  int fd;

  fd = open_listenfd(DAEMON_NAME ".sock");
  if (fd < 0) {
    return -1;
  }

  /* signal handler shim. This is a bit ugly, but so are signals... */
  ctx_ = &ctx;
  sa.sa_handler = &on_sigchld;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  if (sigaction(SIGCHLD, &sa, 0) == -1) {
    perror(0);
    exit(1);
  }

  ret = sc2_init(&ctx, &opts->sc2, fd);
  if (ret != SC2_OK) {
    ylog_error("sc2_init: %s", sc2_strerror(ret));
    return -1;
  }

  ret = sc2_serve(&ctx);
  if (ret != SC2_OK) {
    ylog_error("sc2_serve: %s", sc2_strerror(ret));
    sc2_cleanup(&ctx);
    return -1;
  }

  signal(SIGCHLD, SIG_DFL); /* restore SIGCHLD handler to default */
  ctx_ = NULL;
  sc2_cleanup(&ctx);
  return 0;
}

int main(int argc, char *argv[]) {
  os_t os;
  struct opts opts = {{0}};
  struct os_daemon_opts daemon_opts = {0};
  int status = EXIT_SUCCESS;
  int ret;

  parse_args_or_die(&opts, argc, argv);
  if (opts.no_daemon) {
    ylog_init(DAEMON_NAME, YLOG_STDERR);
    if (chdir(opts.basepath) < 0) {
      ylog_error("chdir %s: %s", opts.basepath, strerror(errno));
      return EXIT_FAILURE;
    }
  } else {
    ylog_init(DAEMON_NAME, YLOG_SYSLOG);
    daemon_opts.flags = opts.daemon_flags;
    daemon_opts.name = DAEMON_NAME;
    daemon_opts.path = opts.basepath;
    daemon_opts.uid = opts.uid;
    daemon_opts.gid = opts.gid;
    daemon_opts.nagroups = 0;
    if (os_daemonize(&os, &daemon_opts) != OS_OK) {
      ylog_error("%s", os_strerror(&os));
      return EXIT_FAILURE;
    }
  }

  ylog_info("Starting " DAEMON_NAME);

  ret = serve(&opts);
  if (ret < 0) {
    ylog_error("failed to serve " DAEMON_NAME);
    status = EXIT_FAILURE;
  }

  if (!opts.no_daemon) {
    ret = os_daemon_remove_pidfile(&os, &daemon_opts);
    if (ret != OS_OK) {
      ylog_error("unable to remove pidfile: %s", os_strerror(&os));
      status = EXIT_FAILURE;
    }
  }

  return status;
}
