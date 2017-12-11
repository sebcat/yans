#!/bin/sh

usage() {
  echo "usage: $0 <name>" >&2
  exit 1
}

[ -n "$1" ] || usage
DAEMON_NAME=$1
DAEMON_DIR=apps/$DAEMON_NAME
mkdir -p "$DAEMON_DIR"
cat <<EOF > "$DAEMON_DIR/main.c"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <lib/util/eds.h>
#include <lib/util/ylog.h>
#include <lib/util/os.h>

#define DAEMON_NAME "$DAEMON_NAME"

struct opts {
  const char *single;
  const char *basepath;
  uid_t uid;
  gid_t gid;
  int no_daemon;
};

static void on_svc_error(struct eds_service *svc, const char *err) {
  ylog_error("%s", err);
}

static void usage() {
  fprintf(stderr,
      "usage:\n"
      "  " DAEMON_NAME " [-s <name>] -u <user> -g <group> -b <basepath>\n"
      "  " DAEMON_NAME " [-s <name>] -n -b <basepath>\n"
      "  " DAEMON_NAME " -h\n"
      "\n"
      "options:\n"
      "  -u, --user:      daemon user\n"
      "  -g, --group:     daemon group\n"
      "  -b, --basepath:  working directory basepath\n"
      "  -s, --single:    name of single service to start\n"
      "  -n, --no-daemon: do not daemonize\n"
      "  -h, --help:      this text\n");
  exit(EXIT_FAILURE);
}

static void parse_args_or_die(struct opts *opts, int argc, char **argv) {
  int ch;
  os_t os;
  static const char *optstr = "u:g:b:ns:h";
  static struct option longopts[] = {
    {"user", required_argument, NULL, 'u'},
    {"group", required_argument, NULL, 'g'},
    {"basepath", required_argument, NULL, 'b'},
    {"single", required_argument, NULL, 's'},
    {"no-daemon", no_argument, NULL, 'n'},
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
      case 's':
        opts->single = optarg;
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
}

int main(int argc, char *argv[]) {
  os_t os;
  struct opts opts = {0};
  struct os_daemon_opts daemon_opts = {0};
  static struct eds_service services[] = {
    {
      .name = "",
      .path = "",
      .udata_size = 0,
      .actions = {
        .on_readable = NULL,
        .on_done = NULL,
      },
      .on_svc_error = on_svc_error,
      .nprocs = 1,
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

  if (opts.single != NULL) {
    ret = eds_serve_single_by_name(services, opts.single);
  } else {
    ret = eds_serve(services);
  }

  if (ret < 0) {
    ylog_error("failed to start %s", opts.single ? opts.single : DAEMON_NAME);
    status = EXIT_FAILURE;
  }

  return status;
}
EOF
