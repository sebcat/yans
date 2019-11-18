/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>

#include <lib/util/eds.h>
#include <lib/util/ylog.h>
#include <lib/util/os.h>

#include <apps/knegd/kng.h>

#define DAEMON_NAME "knegd"

struct opts {
  const char *single;
  const char *basepath;
  const char *knegdir;
  char *queuedir;
  int nqueueslots;
  long timeout;
  uid_t uid;
  gid_t gid;
  int no_daemon;
  const char *storesock;
};

static void on_svc_error(struct eds_service *svc, const char *err) {
  ylog_error("%s", err);
}

static void usage() {
  fprintf(stderr,
      "usage:\n"
      "  " DAEMON_NAME " [opts] -u <user> -g <group> -b <basepath>\n"
      "  " DAEMON_NAME " [opts] -n -b <basepath>\n"
      "  " DAEMON_NAME " -h\n"
      "\n"
      "options:\n"
      "  -u|--user <user>      daemon user\n"
      "  -g|--group <group>    daemon group\n"
      "  -b|--basepath <path>  working directory basepath\n"
      "  -s|--single <name>    name of single service to start\n"
      "  -n|--no-daemon        do not daemonize\n"
      "  -k|--knegdir <path>   path do kneg directory\n"
      "  -q|--queue <path>     path to kneg queue directory\n"
      "  -N|--queue-slots <n>  number of slots in knegd queue\n"
      "  -t|--timeout <secs>   default timeout, in seconds (%d)\n"
      "  -S|--storesock <path> path to store socket (%s)\n"
      "  -h|--help             this text\n",
      DFL_TIMEOUT, DFL_STORESOCK);
  exit(EXIT_FAILURE);
}

static void parse_args_or_die(struct opts *opts, int argc, char **argv) {
  int ch;
  os_t os;
  long l;
  static const char *optstr = "u:g:b:ns:k:q:N:t:S:h";
  static struct option longopts[] = {
    {"user", required_argument, NULL, 'u'},
    {"group", required_argument, NULL, 'g'},
    {"basepath", required_argument, NULL, 'b'},
    {"single", required_argument, NULL, 's'},
    {"no-daemon", no_argument, NULL, 'n'},
    {"knegdir", required_argument, NULL, 'k'},
    {"queue", required_argument, NULL, 'q'},
    {"queue-slots", required_argument, NULL, 'N'},
    {"timeout", required_argument, NULL, 't'},
    {"storesock", required_argument, NULL, 'S'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
  };

  /* init default values */
  opts->basepath = NULL;
  opts->single = NULL;
  opts->knegdir = NULL;
  opts->queuedir = NULL;
  opts->nqueueslots = 0;
  opts->uid = 0;
  opts->gid = 0;
  opts->no_daemon = 0;
  opts->timeout = 0;
  opts->storesock = DFL_STORESOCK;

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
      case 'k':
        opts->knegdir = optarg;
        break;
      case 'q':
        opts->queuedir = optarg;
        break;
      case 'N':
        l = strtol(optarg, NULL, 10);
        if (l <= 0 || l == INT_MAX) {
          fprintf(stderr, "invalid number of slots\n");
          exit(EXIT_FAILURE);
        }
        opts->nqueueslots = (int)l;
        break;
      case 't':
        l = strtol(optarg, NULL, 10);
        if (l <= 0 || l == LONG_MAX) {
          fprintf(stderr, "invalid timeout\n");
          exit(EXIT_FAILURE);
        }
        opts->timeout = l;
        break;
      case 'S':
        opts->storesock = optarg;
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
      .name = DAEMON_NAME,
      .path = DAEMON_NAME ".sock",
      .udata_size = sizeof(struct kng_cli),
      .tick_slice_us = 30 * 1000000,
      .actions = {
        .on_readable = kng_on_readable,
        .on_finalize = kng_on_finalize,
      },
      .on_svc_reaped_child = kng_on_svc_reaped_child,
      .on_svc_error = on_svc_error,
      .on_svc_tick = kng_on_tick,
      .mod_init = kng_mod_init,
      .mod_fini = kng_mod_fini,
      .nprocs = 1,
    },
    {0},
  };
  int status = EXIT_SUCCESS;
  int ret;

  parse_args_or_die(&opts, argc, argv);

  kng_set_storesock(opts.storesock);
  if (opts.knegdir != NULL) {
    kng_set_knegdir(opts.knegdir);
  }

  if (opts.queuedir != NULL) {
    kng_set_queuedir(opts.queuedir);
  }

  if (opts.nqueueslots > 0) {
    kng_set_nqueueslots(opts.nqueueslots);
  }

  if (opts.timeout > 0) {
    kng_set_timeout(opts.timeout);
  }

  if (opts.no_daemon) {
    ylog_init(DAEMON_NAME, YLOG_STDERR);
    if (chdir(opts.basepath) < 0) {
      ylog_error("chdir %s: %s", opts.basepath, strerror(errno));
      return EXIT_FAILURE;
    }
  } else {
    ylog_init(DAEMON_NAME, YLOG_SYSLOG);
    /* no chroot because we want to be able to execve arbitrary binaries */
    daemon_opts.flags = DAEMONOPT_NOCHROOT;
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

  ylog_info("Starting %s", opts.single ? opts.single : DAEMON_NAME);

  if (opts.single != NULL) {
    ret = eds_serve_single_by_name(services, opts.single);
  } else {
    ret = eds_serve(services);
  }

  if (ret < 0) {
    ylog_error("failed to serve %s", opts.single ? opts.single : DAEMON_NAME);
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
