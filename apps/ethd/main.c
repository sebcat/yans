/* vim: set tabstop=2 shiftwidth=2 expandtab ai: */
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#ifdef __linux__
#include <sys/prctl.h>
#include <sys/capability.h>
#endif

#include <lib/util/os.h>
#include <lib/util/ylog.h>
#include <lib/util/eds.h>

#include <apps/ethd/pcap.h>
#include <apps/ethd/ethframe.h>
#include <apps/ethd/sweeper.h>
#include <apps/ethd/sender.h>

#define DAEMON_NAME       "ethd"

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
      "  ethd [-s <name>] -u <user> -g <group> -b <basepath>\n"
      "  ethd [-s <name>] -n -b <basepath>\n"
      "  ethd -h\n"
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

void chroot_or_die(struct opts *opts) {
  os_t os;
  struct os_chrootd_opts chroot_opts;

  /* for Linux: Set the "keep capabilities" flag to 1 and drop the permitted,
   * inheritable and effective capabilities to only include the ones needed
   * to set up the chroot, as well as CAP_NET_RAW. */
#ifdef __linux__
  do {
    cap_t caps;
    cap_value_t newcaps[] = {
      CAP_NET_RAW,
      CAP_CHOWN,
      CAP_FOWNER,
      CAP_DAC_OVERRIDE,
      CAP_SETGID,
      CAP_SETUID,
      CAP_SYS_CHROOT,
    };
    prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
    caps = cap_init(); /* cap_init == all flags cleared */
    cap_set_flag(caps, CAP_PERMITTED, sizeof(newcaps) / sizeof(cap_value_t),
        newcaps, CAP_SET);
    cap_set_flag(caps, CAP_EFFECTIVE, sizeof(newcaps) / sizeof(cap_value_t),
        newcaps, CAP_SET);
    cap_set_flag(caps, CAP_INHERITABLE, sizeof(newcaps) / sizeof(cap_value_t),
        newcaps, CAP_SET);
    if (cap_set_proc(caps) < 0) {
      cap_free(caps);
      ylog_error("cap_set_proc pre-chroot: %s", strerror(errno));
      exit(EXIT_FAILURE);
    }
    cap_free(caps);
  } while(0);
#endif

  chroot_opts.name = DAEMON_NAME;
  chroot_opts.path = opts->basepath;
  chroot_opts.uid = opts->uid;
  chroot_opts.gid = opts->gid;
  chroot_opts.nagroups = 0;
  if (os_chrootd(&os, &chroot_opts) != OS_OK) {
    ylog_error("%s", os_strerror(&os));
    exit(EXIT_FAILURE);
  }
  /* For Linux: After chrooting, drop the capabilities that was needed to
   * establish the chroot from the permitted set while keeping CAP_NET_RAW.
   * Set CAP_NET_RAW in the effective set which was cleared on setuid() */
#ifdef __linux__
  do {
    cap_t caps;
    cap_value_t newcaps[] = {CAP_NET_RAW};
    caps = cap_init();
    cap_set_flag(caps, CAP_PERMITTED, 1, newcaps, CAP_SET);
    cap_set_flag(caps, CAP_EFFECTIVE, 1, newcaps, CAP_SET);
    if (cap_set_proc(caps) < 0) {
      cap_free(caps);
      ylog_error("cap_set_proc post-chroot: %s", strerror(errno));
      exit(EXIT_FAILURE);
    }
    cap_free(caps);
  } while(0);
#endif
}

static void on_svc_error(struct eds_service *svc, const char *err) {
  ylog_error("%s", err);
}

int main(int argc, char *argv[]) {
  static struct eds_service services[] = {
    {
      .name = "pcap",
      .path = "pcap.sock",
      .udata_size = sizeof(struct pcap_client),
      .actions = {
        .on_readable = pcap_on_readable,
        .on_done = pcap_on_done,
      },
      .nprocs = 2,
      .on_svc_error = on_svc_error,
    },
    {
      .name = "ethframe",
      .path = "ethframe.sock",
      .mod_init = ethframe_init,
      .mod_fini = ethframe_fini,
      .udata_size = sizeof(struct ethframe_client),
      .actions = {
        .on_readable = ethframe_on_readable,
        .on_done = ethframe_on_done,
      },
      .nprocs = 1,
      .on_svc_error = on_svc_error,
    },
    {
      .name = "sweeper",
      .path = "sweeper.sock",
      .udata_size = sizeof(struct sweeper_client),
      .actions = {
        .on_readable = sweeper_on_readable,
        .on_done = sweeper_on_done,
      },
      .nprocs = 1,
      .tick_slice_us = 50000,
      .on_svc_error = on_svc_error,
    },
    {
      .name = "sender",
      .path = "sender.sock",
      .mod_init = sender_init,
      .mod_fini = sender_fini,
      .actions = {
        .on_readable = sender_on_readable,
        .on_done = sender_on_done,
      },
      .on_svc_error = on_svc_error,
    },
    {0},
  };
  struct opts opts;
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
    chroot_or_die(&opts);
  }

  if (opts.single != NULL) {
    ylog_info("Starting service: %s", opts.single);
    if (eds_serve_single_by_name(services, opts.single) < 0) {
      ylog_error("unable to start service \"%s\"", opts.single);
      status = EXIT_FAILURE;
    }
  } else {
    ylog_info("Starting ethd services");
    if (eds_serve(services) < 0) {
      ylog_error("eds_serve: failed");
      status = EXIT_FAILURE;
    }
  }

  return status;
}
