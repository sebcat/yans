#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

#include <lib/net/fcgi.h>

#include <lib/util/ylog.h>
#include <lib/util/eds.h>
#include <lib/util/os.h>

#define DAEMON_NAME "fcgi-demo"

struct opts {
  const char *basepath;
  const char *single;
  uid_t uid;
  gid_t gid;
  int no_daemon;
};

static void usage() {
  fprintf(stderr,
      "usage:\n"
      "  fcgi-demo [-s <name>] -u <user> -g <group> -b <basepath>\n"
      "  fcgi-demo [-s <name>] -n -b <basepath>\n"
      "  fcgi-demo -h\n"
      "\n"
      "options:\n"
      "  -u, --user:      daemon user\n"
      "  -g, --group:     daemon group\n"
      "  -b, --basepath:  chroot basepath\n"
      "  -n, --no-daemon: do not daemonize\n"
      "  -s, --single:    start a single service, indicated by name\n"
      "  -h, --help:      this text\n");
  exit(EXIT_FAILURE);
}

static int parse_args_or_die(struct opts *opts, int argc, char **argv) {
  int ch;
  os_t os;
  static const char *optstr = "u:g:b:ns:h";
  static struct option longopts[] = {
    {"user", required_argument, NULL, 'u'},
    {"group", required_argument, NULL, 'g'},
    {"basepath", required_argument, NULL, 'b'},
    {"no-daemon", no_argument, NULL, 'n'},
    {"single", required_argument, NULL, 's'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
  };

  /* init default values */
  opts->basepath = NULL;
  opts->single = NULL;
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
      case 's':
        opts->single = optarg;
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
  return 0;
}

struct demo_ctx {
  struct fcgi_cli fcgi;
  buf_t resp;
  struct fcgi_end_request end;
};

#define DEMO_CTX(cli) \
    ((struct demo_ctx *)((cli)->udata))

#define RESP_HEAD \
 "Content-Type: text/plain\r\n\r\n"

static void on_write_done(struct eds_client *cli, int fd) {
  struct demo_ctx *ctx = DEMO_CTX(cli);
  fcgi_format_endmsg(&ctx->end, 1, 0);
  eds_client_send(cli, (char*)&ctx->end, sizeof(ctx->end), NULL);
  eds_client_clear_actions(cli);
}

static void on_read_req(struct eds_client *cli, int fd) {
  struct fcgi_pair pair;
  size_t off = 0;
  struct demo_ctx *ctx = DEMO_CTX(cli);
  struct fcgi_cli *fcgi = &ctx->fcgi;
  int ret;
  struct eds_transition trans = {
    .flags = EDS_TFLWRITE,
    .on_writable = on_write_done,
  };

  ret = fcgi_cli_readreq(fcgi);
  if (ret == FCGI_OK) {
    buf_init(&ctx->resp, 4096);
    buf_adata(&ctx->resp, "\0\0\0\0\0\0\0\0", 8); /* header allocation */
    buf_adata(&ctx->resp, RESP_HEAD, sizeof(RESP_HEAD)-1);
    while (fcgi_cli_next_param(fcgi, &off, &pair) == FCGI_AGAIN) {
      buf_adata(&ctx->resp, pair.key, pair.keylen);
      buf_adata(&ctx->resp, ": ", 2);
      buf_adata(&ctx->resp, pair.value, pair.valuelen);
      buf_achar(&ctx->resp, '\n');
    }

    if (ctx->resp.len > 65535) {
      ctx->resp.len = 65535;
    }
    fcgi_format_header((struct fcgi_header *)ctx->resp.data, FCGI_STDOUT,
        1, ctx->resp.len-8);
    eds_client_set_on_readable(cli, NULL);
    fprintf(stderr, "%.*s", (int)ctx->resp.len, ctx->resp.data+8);
    eds_client_send(cli, ctx->resp.data, ctx->resp.len, &trans);
  } else if (ret == FCGI_ERR) {
    ylog_error("%scli%d: read_req: %s", cli->svc->name, fd,
        fcgi_cli_strerror(fcgi));
    eds_client_clear_actions(cli);
  }
}

static void on_readable(struct eds_client *cli, int fd) {
  struct demo_ctx *ctx = DEMO_CTX(cli);
  struct fcgi_cli *fcgi = &ctx->fcgi;
  fcgi_cli_init(fcgi, fd);
  eds_client_set_on_readable(cli, on_read_req);
  on_read_req(cli, fd);
}

static void on_done(struct eds_client *cli, int fd) {
  struct demo_ctx *ctx = DEMO_CTX(cli);

  fcgi_cli_cleanup(&ctx->fcgi);
  buf_cleanup(&ctx->resp);
  ylog_info("%scli%d: done", cli->svc->name, fd);
}

static void on_svc_error(struct eds_service *svc, const char *err) {
  ylog_error("%s", err);
}

int main(int argc, char *argv[]) {
  os_t os;
  struct opts opts = {0};
  struct os_chrootd_opts chroot_opts;
  static struct eds_service services[] = {
    {
      .name = "fcgi-demo",
      .path = "fcgi-demo.sock",
      .udata_size = sizeof(struct demo_ctx),
      .actions = {
        .on_readable = on_readable,
        .on_done = on_done,
      },
      .nprocs = 2,
      .nfds = 256,
      .on_svc_error = on_svc_error,
    },
    {NULL, NULL}
  };
  int status = EXIT_SUCCESS;

  parse_args_or_die(&opts, argc, argv);
  if (opts.no_daemon) {
    ylog_init(DAEMON_NAME, YLOG_STDERR);
    if (chdir(opts.basepath) < 0) {
      ylog_error("chdir %s: %s", opts.basepath, strerror(errno));
      return EXIT_FAILURE;
    }
  } else {
    ylog_init(DAEMON_NAME, YLOG_SYSLOG);
    chroot_opts.name = DAEMON_NAME;
    chroot_opts.path = opts.basepath;
    chroot_opts.uid = opts.uid;
    chroot_opts.gid = opts.gid;
    chroot_opts.nagroups = 0;
    if (os_chrootd(&os, &chroot_opts) != OS_OK) {
      ylog_error("%s", os_strerror(&os));
      exit(EXIT_FAILURE);
    }
  }

  if (opts.single != NULL) {
    ylog_info("Starting service: %s", opts.single);
    if (eds_serve_single_by_name(services, opts.single) < 0) {
      ylog_error("unable to start service \"%s\"", opts.single);
      status = EXIT_FAILURE;
    }
  } else {
    ylog_info("Starting fcgi-demo services");
    if (eds_serve(services) < 0) {
      ylog_error("eds_serve: failed");
      status = EXIT_FAILURE;
    }
  }

  return status;

  return EXIT_FAILURE;
}
