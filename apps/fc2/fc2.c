#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>

#include <lib/util/os.h>
#include <lib/util/ylog.h>
#include <lib/util/eds.h>
#include <lib/net/fcgi.h>

#define DAEMON_NAME "fc2"
#define CGIPATH_MAX 256

struct opts {
  const char *basepath;
  uid_t uid;
  gid_t gid;
  int no_daemon;
};

struct fc2_ctx {
  int exit_status;
  struct eds_client *child_cli;
  pid_t cgi_pid;
  io_t cgi_io;
  struct fcgi_cli fcgi;

  /* holds {FCGI_STDOUT,0} and {FCGI_END_REQUEST, {...}} */
  char teardown_buf[sizeof(struct fcgi_header) +
      sizeof(struct fcgi_end_request)];

  /* data from the CGI process - will be written as FCGI_STDOUT data */
  buf_t outbuf;
};

struct fc2_cgi {
  struct eds_client *parent;
};

/* why do I do this to myself? */
#define FC2_CTX(cli__) \
    ((struct fc2_ctx*)((cli__)->udata))

#define FC2_CGI(cli__) \
    ((struct fc2_cgi*)((cli__)->udata))

#define CLIERR(cli__, fd, fmt__, ...) \
    ylog_error("%scli%d: " fmt__ , (cli__)->svc->name, (fd), __VA_ARGS__)

#define CLINFO(cli__, fd, fmt__, ...) \
    ylog_info("%scli%d: " fmt__ , (cli__)->svc->name, (fd), __VA_ARGS__)

static char *g_cgidir;

static const char r404[] =
  "\x00\x00\x00\x00\x00\x00\x00\x00" /* copy header here */
  "Status: 404 Not Found\r\n"
  "Content-Type: text/plain\r\n"
  "\r\n"
  "404 Not Found";

struct cgi_env {
  char full_cgi_path[CGIPATH_MAX]; /* path to executed CGI file */
  int cgifd;
  buf_t envp_buf;
};

#define CGIERR_OK                0
#define CGIERR_MEMORY           -1
#define CGIERR_OPEN             -2
#define CGIERR_INVALIDPATH      -3
#define CGIERR_UNKNOWN        -255

#define REQUEST_URI     "REQUEST_URI"
#define REQUEST_URI_LEN            11

static const char *cgi_strerror(int code) {
  switch (code) {
  case CGIERR_OK:
    return "success";
  case CGIERR_MEMORY:
    return "memory allocation error";
  case CGIERR_OPEN:
    return "unable to open/execute CGI";
  case CGIERR_INVALIDPATH:
    return "invalid CGI path";
  default:
    return "unknown CGI error";
  }
}

static void clean_cgi_env(struct cgi_env *env) {
  char **curr;
  size_t len = 0;

  if (env->cgifd != -1) {
    close(env->cgifd);
  }

  if (env->envp_buf.cap > 0) {
    while (len < env->envp_buf.len) {
      curr = (char**)(env->envp_buf.data + len);
      if (*curr == NULL) {
        break;
      }
      free(*curr);
      len += sizeof(char*);
    }
    buf_cleanup(&env->envp_buf);
  }
}

static int setup_cgi_env(struct fcgi_cli *fcgi, struct cgi_env *out) {
  size_t len;
  char *cptr;
  struct fcgi_pair pair;
  size_t off = 0;
  int status = CGIERR_UNKNOWN;
  char cgi_path[CGIPATH_MAX];

  cgi_path[0] = '\0';
  out->full_cgi_path[0] = '\0';
  out->cgifd = -1;
  buf_init(&out->envp_buf, 2048);
  while (fcgi_cli_next_param(fcgi, &off, &pair) == FCGI_AGAIN) {
    /* check for REQUEST_URI */
    if (pair.keylen == REQUEST_URI_LEN &&
        strncmp(pair.key, REQUEST_URI, REQUEST_URI_LEN) == 0) {
      for (len = 0; len < pair.valuelen && pair.value[len] != '?'; len++);
      if (len > CGIPATH_MAX-1) {
        status = CGIERR_INVALIDPATH;
        goto fail;
      }
      snprintf(cgi_path, sizeof(cgi_path), "%.*s",
          (int)len, pair.value);
      os_cleanpath(cgi_path);
    }

    /* Setup the envp string for this parameter.
     * We prefix the variables with FC2_ to avoid silly things like a
     * supplied HTTP Proxy: header expanding to HTTP_PROXY, &c */
    len = pair.keylen + pair.valuelen + 6; /* FC2_<KEY>=<VAL>\0 */
    cptr = malloc(len);
    if (cptr == NULL) {
      status = CGIERR_MEMORY;
      goto fail;
    }
    snprintf(cptr, len, "FC2_%.*s=%.*s", (int)pair.keylen, pair.key,
        (int)pair.valuelen, pair.value);
    buf_adata(&out->envp_buf, &cptr, sizeof(char*));
  }
  cptr = NULL;
  buf_adata(&out->envp_buf, &cptr, sizeof(char*));

  if (cgi_path[0] != '/') {
    status = CGIERR_INVALIDPATH;
    goto fail;
  }

  snprintf(out->full_cgi_path, sizeof(out->full_cgi_path), "%s%s",
      g_cgidir, cgi_path);
  out->cgifd = open(out->full_cgi_path, O_RDONLY);
  if (out->cgifd < 0) {
    status = CGIERR_OPEN;
    goto fail;
  }

  /* check to see if the file is a regular file */
  if (!os_fdisfile(out->cgifd)) {
    status = CGIERR_OPEN;
    goto fail;
  }

  return CGIERR_OK;

fail:
  clean_cgi_env(out);
  return status;
}

static pid_t fork_reqproc(struct eds_client *cli, int fd, struct cgi_env *env,
    int *procfd) {
  int fds[2] = {0};
  int ret;
  pid_t pid;
  char *cgi_argv[2] = {0};

  ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
  if (ret < 0) {
    CLIERR(cli, fd, "socketpair failure: %s", strerror(errno));
    goto fail_prefork;
  }

  pid = fork();
  if (pid < 0) {
    CLIERR(cli, fd, "fork failure: %s", strerror(errno));
    goto fail_prefork;
  } else if (pid > 0) {
    *procfd = fds[0];
    close(fds[1]);
    return pid;
  }

  /* setup child file descriptors */
  close(fds[0]);
  close(cli->svc->cmdfd);
  dup2(fds[1], STDIN_FILENO);
  dup2(fds[1], STDOUT_FILENO);
  dup2(fds[1], STDERR_FILENO);
  close(fds[1]);

  /* fexecve! */
  cgi_argv[0] = env->full_cgi_path;
  ret = fexecve(env->cgifd, cgi_argv, (void*)env->envp_buf.data);
  /* we can't log this here - stderr will redirect to client */
  /* CLIERR(cli, fd, "fexecve: %s", strerror(errno)); */
  clean_cgi_env(env);
  exit(1);

fail_prefork:
  if (fds[0] != 0) {
    close(fds[0]);
    close(fds[1]);
  }

  return -1;
}

static void on_childproc_readable(struct eds_client *cli, int fd);
static void on_fcgi_msg(struct eds_client *cli, int fd);

static void on_reactivate_fcgi(struct eds_client *cli, int fd) {
  struct fc2_cgi *cgi = FC2_CGI(cli);
  struct fc2_ctx *ctx = FC2_CTX(cgi->parent);

  eds_client_set_on_writable(ctx->child_cli, NULL, 0);
  eds_client_set_on_readable(cgi->parent, on_fcgi_msg, 0);
}

static void on_fcgi_msg(struct eds_client *cli, int fd) {
  struct eds_transition trans;
  struct fc2_ctx *ctx = FC2_CTX(cli);
  struct fcgi_msg msg;
  int ret;
  int child_fd;

  /* read an FCGI message */
  ret = fcgi_cli_readmsg(&ctx->fcgi, &msg);
  if (ret == FCGI_AGAIN) {
    return;
  } else if (ret == FCGI_ERR) {
    CLIERR(cli, fd, "fcgi_cli_readmsg: %s", fcgi_cli_strerror(&ctx->fcgi));
    goto done;
  }

  if (msg.header->t != FCGI_STDIN) {
    CLINFO(cli, fd, "got %s, shutting down",
        fcgi_type_to_str(msg.header->t));
    goto done;
  }

  if (FCGI_CLEN(msg.header) == 0) {
    /* FCGI_STDIN, 0 == end-of-stream */
    child_fd = eds_client_get_fd(ctx->child_cli);
    shutdown(child_fd, SHUT_WR);
    eds_client_set_on_writable(ctx->child_cli, NULL, 0);
  } else {
    /* suspend further reading until writing is done */
    eds_client_suspend_readable(cli);
    trans.flags = EDS_TFLWRITE;
    trans.on_writable = on_reactivate_fcgi;
    eds_client_send(ctx->child_cli, msg.data, FCGI_CLEN(msg.header), &trans);
  }

  return;
done:
  eds_service_remove_client(cli->svc, ctx->child_cli);
  eds_service_remove_client(cli->svc, cli);
  return;
}

static void on_reactivate_childproc(struct eds_client *cli, int fd) {
  struct fc2_ctx *ctx = FC2_CTX(cli);
  buf_truncate(&ctx->outbuf, sizeof(struct fcgi_header));
  eds_client_set_on_writable(cli, NULL, 0);
  eds_client_set_on_readable(ctx->child_cli, on_childproc_readable, 0);
}

static void on_graceful_teardown_done(struct eds_client *cli, int fd) {
  eds_service_remove_client(cli->svc, cli);
}

static void on_graceful_teardown(struct eds_client *cli, int fd) {
  struct eds_transition trans;
  struct fc2_ctx *ctx = FC2_CTX(cli);
  struct fcgi_header *out0 = (struct fcgi_header *)ctx->teardown_buf;
  struct fcgi_end_request *ereq =(struct fcgi_end_request*)
      (ctx->teardown_buf + sizeof(struct fcgi_header));

  /* NB: child may not yet have been reaped. If not, we default to 0 */
  fcgi_cli_format_header(&ctx->fcgi, out0, FCGI_STDOUT, 0);
  fcgi_cli_format_endmsg(&ctx->fcgi, ereq,
      ctx->exit_status >= 0 ? ctx->exit_status : 0);

  trans.flags = EDS_TFLWRITE;
  trans.on_writable = on_graceful_teardown_done;
  eds_client_send(cli, ctx->teardown_buf, sizeof(ctx->teardown_buf), &trans);
}

static void on_childproc_readable(struct eds_client *cli, int fd) {
  struct fc2_cgi *cgi = FC2_CGI(cli);
  struct fc2_ctx *ctx = FC2_CTX(cgi->parent);
  struct eds_transition trans;
  size_t nread;
  int ret;

  /* read a chunk of data */
  ret = io_readbuf(&ctx->cgi_io, &ctx->outbuf, &nread);
  if (ret == IO_ERR) {
    CLIERR(cgi->parent, fd, "io_readbuf: %s", io_strerror(&ctx->cgi_io));
    goto done;
  } else if (ret == IO_AGAIN) {
    return;
  } else if (nread == 0) {
    /* send FCI_STDOUT 0 and FCGI_END_REQUEST */
    eds_client_suspend_readable(cli);
    eds_client_suspend_readable(cgi->parent);
    /* defer the calling of the handler to the event loop */
    eds_client_set_on_writable(cgi->parent, on_graceful_teardown, EDS_DEFER);
    return;
  }

  /* the size of the buffer dictates the number of received bytes. If it is
   * ever increased, we risk losing data. This assert guards against that. */
  assert(nread < 65527);

  /* set up the FCGI_STDOUT header */
  fcgi_cli_format_header(&ctx->fcgi, (void*)ctx->outbuf.data, FCGI_STDOUT,
      (unsigned short)nread);

  /* suspend reading from childproc until outbuf is sent */
  eds_client_suspend_readable(cli);

  /* write the content of outbuf to the cgi parent */
  trans.flags = EDS_TFLWRITE;
  trans.on_writable = on_reactivate_childproc;
  eds_client_send(cgi->parent, ctx->outbuf.data, ctx->outbuf.len, &trans);

  return;
done:
  eds_service_remove_client(cli->svc, cgi->parent);
  eds_service_remove_client(cli->svc, cli);
  return;
}

static void on_childproc_done(struct eds_client *cli, int fd) {
  /* if e.g., eds_client_send fails in the middle of a write on childproc,
   * we need to signal that to the parent. We do that here. */
  struct fc2_cgi *cgi = FC2_CGI(cli);
  struct eds_client *pcli = cgi->parent;
  eds_service_remove_client(cli->svc, pcli);
}

static void on_read_req(struct eds_client *cli, int fd) {
  struct fc2_ctx *ctx = FC2_CTX(cli);
  struct eds_client_actions acts = {0};
  struct fc2_cgi *ncli;
  struct cgi_env env = {{0}};
  struct eds_transition trans;
  int ret;
  int procfd;
  pid_t pid;

  ret = fcgi_cli_readparams(&ctx->fcgi);
  if (ret == FCGI_OK) {

    /* setup the CGI environment */
    ret = setup_cgi_env(&ctx->fcgi, &env);
    if (ret != CGIERR_OK) {
      if (env.full_cgi_path[0] != '\0') {
        CLIERR(cli, fd, "setup_cgi_env: %s (%s)", cgi_strerror(ret),
            env.full_cgi_path);
      } else {
        CLIERR(cli, fd, "setup_cgi_env: %s", cgi_strerror(ret));
      }

      /* respond with 404 (TODO: support other HTTP error codes) */
      /* init response buffer */
      if (buf_init(&ctx->outbuf, 2048) == NULL) {
        CLIERR(cli, fd, "%s", "unable to allocate space for error response");
        eds_client_clear_actions(cli);
        return;
      }

      /* craft 404 response (TODO: support other HTTP error codes) */
      buf_adata(&ctx->outbuf, r404, sizeof(r404)-1);
      fcgi_cli_format_header(&ctx->fcgi, (void*)ctx->outbuf.data, FCGI_STDOUT,
          sizeof(r404)-1-8);

      /* send 404 response and transition to graceful teardown */
      eds_client_set_on_readable(cli, NULL, 0);
      trans.flags = EDS_TFLWRITE;
      trans.on_writable = on_graceful_teardown;
      eds_client_send(cli, ctx->outbuf.data, ctx->outbuf.len, &trans);
      return;
    }

    /* fork the child process */
    pid = fork_reqproc(cli, fd, &env, &procfd);
    if (pid < 0) {
      clean_cgi_env(&env);
      CLIERR(cli, fd, "%s", "fork_reqproc failure");
      eds_client_clear_actions(cli);
      return;
    }

    IO_INIT(&ctx->cgi_io, procfd);
    CLINFO(cli, fd, "reqproc PID:%d fd:%d path:%s", pid, procfd,
        env.full_cgi_path);
    clean_cgi_env(&env);
    acts.on_readable = on_childproc_readable;
    acts.on_done = on_childproc_done;
    if ((ctx->child_cli = eds_service_add_client(cli->svc, procfd,
        &acts)) == NULL) {
      CLIERR(cli, fd, "PID:%d fd:%d: failed to add client", pid, procfd);
      goto fail_postfork;
    }

    ncli = FC2_CGI(ctx->child_cli);
    ncli->parent = cli;
    if (buf_init(&ctx->outbuf, 65536/2) == NULL) {
      CLIERR(cli, fd, "PID:%d fd:%d: failed to allocate output buffer",
          pid, procfd);
      goto fail_postfork;
    }
    /* reserve space for struct fcgi_header */
    buf_adata(&ctx->outbuf, "\0\0\0\0\0\0\0\0", 8);
    ctx->cgi_pid = pid;
    eds_client_set_on_readable(cli, on_fcgi_msg, 0);
  } else if (ret == FCGI_ERR) {
    CLIERR(cli, fd, "read_req: %s", fcgi_cli_strerror(&ctx->fcgi));
    eds_client_clear_actions(cli);
  }
  return;

fail_postfork:
  /* procfd will be closed by on_done */
  kill(pid, SIGKILL);
  eds_client_clear_actions(cli);
  return;
}

static void on_readable(struct eds_client *cli, int fd) {
  struct fc2_ctx *ctx;

  ctx = FC2_CTX(cli);
  ctx->exit_status = -1;
  fcgi_cli_init(&ctx->fcgi, fd);
  eds_client_set_on_readable(cli, on_read_req, 0);
}

static void on_done(struct eds_client *cli, int fd) {
  struct fc2_ctx *ctx = FC2_CTX(cli);
  fcgi_cli_cleanup(&ctx->fcgi);
  buf_cleanup(&ctx->outbuf);
  if (ctx->cgi_pid > 0 && ctx->exit_status < 0) {
    kill(ctx->cgi_pid, SIGKILL);
  }
  if (ctx->child_cli != NULL) {
    eds_service_remove_client(cli->svc, ctx->child_cli);
    ctx->child_cli = NULL;
  }
  ylog_info("%scli%d: done", cli->svc->name, fd);
}

static void on_cli_reaped_child(struct eds_service *svc,
    struct eds_client *cli, pid_t pid, int status) {
  struct fc2_ctx *ctx = ctx = FC2_CTX(cli);
  if (ctx->cgi_pid == pid) {
    ctx->exit_status = status;
  }
}

static void on_svc_error(struct eds_service *svc, const char *err) {
  ylog_error("%s", err);
}

static void usage() {
  fprintf(stderr,
      "usage:\n"
      "  fc2 -u <user> -g <group> -b <basepath> </path/to/cgi/dir>\n"
      "  fc2 -n -b <basepath> </path/to/cgi/dir>\n"
      "  fc2 -h\n"
      "\n"
      "options:\n"
      "  -u, --user:      daemon user\n"
      "  -g, --group:     daemon group\n"
      "  -b, --basepath:  working directory basepath\n"
      "  -n, --no-daemon: do not daemonize\n"
      "  -h, --help:      this text\n");
  exit(EXIT_FAILURE);
}

static int parse_args_or_die(struct opts *opts, int argc, char **argv) {
  int ch;
  os_t os;
  static const char *optstr = "u:g:b:nh";
  static struct option longopts[] = {
    {"user", required_argument, NULL, 'u'},
    {"group", required_argument, NULL, 'g'},
    {"basepath", required_argument, NULL, 'b'},
    {"no-daemon", no_argument, NULL, 'n'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
  };

  /* init default values */
  opts->basepath = NULL;
  opts->uid = 0;
  opts->gid = 0;
  opts->no_daemon = 0;

  while ((ch = getopt_long(argc, argv, optstr, longopts, NULL)) != -1) {
    switch (ch) {
      case 'u':
        if (os_getuid(&os, optarg, &opts->uid) != OS_OK) {
          fprintf(stderr, "%s\n", os_strerror(&os));
          exit(EXIT_FAILURE);
        }
        break;
      case 'g':
        if(os_getgid(&os, optarg, &opts->gid) != OS_OK) {
          fprintf(stderr, "%s\n", os_strerror(&os));
          exit(EXIT_FAILURE);
        }
        break;
      case 'b':
        opts->basepath = optarg;
        break;
      case 'n':
        opts->no_daemon = 1;
        break;
      case 'h':
      default:
        usage();
    }
  }

  /* sanity check opts */
  if (opts->basepath == NULL) {
    usage();
  } else if (opts->basepath[0] != '/') {
    fprintf(stderr, "basepath must be an absolute path\n");
    exit(EXIT_FAILURE);
  } else if (opts->no_daemon == 0 && (opts->gid == 0 || opts->uid == 0)) {
    fprintf(stderr, "daemon must run as unprivileged user:group\n");
    exit(EXIT_FAILURE);
  }

  argc -= optind;
  argv += optind;
  if (argc <= 0) {
    fprintf(stderr, "missing CGI workdir\n");
    usage();
  }

  /* setup and validate g_cgidir */
  g_cgidir = os_cleanpath(argv[0]);
  if (*g_cgidir != '/') {
    fprintf(stderr, "CGI workdir must be an absolute path");
    usage();
  } else if (!os_isdir(g_cgidir)) {
    fprintf(stderr, "CGI workdir is not a valid directory");
    usage();
  }

  return 0;
}

int main(int argc, char *argv[]) {
  os_t os;
  struct opts opts = {0};
  struct os_daemon_opts daemon_opts = {0};
  static struct eds_service services[] = {
    {
      .name = DAEMON_NAME,
      .path = DAEMON_NAME ".sock",
      .udata_size = sizeof(struct fc2_ctx),
      .actions = {
        .on_readable = on_readable,
        .on_done = on_done,
      },
      .on_svc_error = on_svc_error,
      .on_cli_reaped_child = on_cli_reaped_child,
      /* in daemon mode we'll have multiple processes to handle slow readers.
       * from a performance standpoint, forking and running the CGIs
       * themselves will probably be a larger bottle neck than the fc2
       * processes */
      .nprocs = 2,
    },
    {0},
  };
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
    /* we want to be able to execve arbitrary CGIs, so no chroot */
    daemon_opts.flags = DAEMONOPT_NOCHROOT;
    daemon_opts.name = DAEMON_NAME;
    daemon_opts.path = opts.basepath;
    daemon_opts.uid = opts.uid;
    daemon_opts.gid = opts.gid;
    daemon_opts.nagroups = 0;
    if (os_daemonize(&os, &daemon_opts) != OS_OK) {
      ylog_error("%s", os_strerror(&os));
      exit(EXIT_FAILURE);
    }
  }

  ylog_info("Starting " DAEMON_NAME);

  /* only do process supervision and multiple workers if we're running as a
   * daemon */
  if (opts.no_daemon) {
    ret = eds_serve_single_by_name(services, DAEMON_NAME);
  } else {
    ret = eds_serve(services);
  }

  if (ret < 0) {
    ylog_error(DAEMON_NAME ": eds service failure");
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
