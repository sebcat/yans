#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <lib/util/os.h>
#include <lib/util/io.h>
#include <lib/util/ylog.h>

#define DAEMON_NAME "pcapd"
#define DAEMON_SOCK "pcapd.sock"
#define MAX_ACCEPT_RETRIES 3

typedef struct pcapcli_t {
  int flags;
  int id;
  pthread_t tid;
  io_t io;
} pcapcli_t;

static void pcapcli_free(pcapcli_t *cli) {
  if (cli != NULL) {
    io_close(&cli->io);
  }
}

static void *pcapcli_main(void *arg) {
  pcapcli_t *cli = arg;

  ylog_info("pcap client finished (id:%d)", cli->id);
  pcapcli_free(cli);
  return NULL;
}

static pcapcli_t *pcapcli_new(io_t *client, int id) {
  pcapcli_t *cli;

  if ((cli = malloc(sizeof(pcapcli_t))) == NULL) {
    return NULL;
  }
  cli->flags = 0;
  memcpy(&cli->io, client, sizeof(io_t));
  cli->id = id;
  if (pthread_create(&cli->tid, NULL, pcapcli_main, cli) != 0) {
    free(cli);
    return NULL;
  }
  return cli;
}

static void accept_loop(io_t *listener) {
  int id_counter = 0, retries = 0;
  io_t client;
  while(1) {
    if (io_accept(listener, &client) != IO_OK) {
      ylog_error("io_accept: %s", io_strerror(listener));
      if (retries >= MAX_ACCEPT_RETRIES) {
        break;
      }
      retries++;
      continue;
    }

    retries = 0;
    if (pcapcli_new(&client, id_counter) == NULL) {
      ylog_perror("pcapcli_new");
      io_close(&client);
    } else {
      ylog_info("pcap client started (id:%d)", id_counter);
      id_counter++;
    }
  }
  ylog_error("accept: maximum number of retries reached");
}

struct pcapd_opts {
  const char *basepath;
  uid_t uid;
  gid_t gid;
  int no_daemon;
};

static void usage() {
  fprintf(stderr,
      "usage:\n"
      "  pcapd -u user -g group -b basepath\n"
      "  pcapd -n -b basepath\n"
      "  pcapd -h\n"
      "\n"
      "options:\n"
      "  -u, --user:      daemon user\n"
      "  -g, --group:     daemon group\n"
      "  -b, --basepath:  chroot basepath\n"
      "  -n, --no-daemon: do not daemonize\n"
      "  -h, --help:      this text\n");
  exit(EXIT_FAILURE);
}

static int parse_args_or_die(struct pcapd_opts *opts, int argc, char **argv) {
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
  return 0;
}

int main(int argc, char *argv[]) {
  struct pcapd_opts opts;
  os_t os;
  io_t io;

  parse_args_or_die(&opts, argc, argv);
  if (opts.no_daemon) {
    ylog_init(DAEMON_NAME, YLOG_STDERR);
    if (chdir(opts.basepath) < 0) {
      ylog_error("chdir %s: %s", opts.basepath, strerror(errno));
      goto end;
    }
  } else {
    struct os_chrootd_opts chroot_opts;
    ylog_init(DAEMON_NAME, YLOG_SYSLOG);
    chroot_opts.name = DAEMON_NAME;
    chroot_opts.path = opts.basepath;
    chroot_opts.uid = opts.uid;
    chroot_opts.gid = opts.gid;
    chroot_opts.nagroups = 0;
    if (os_chrootd(&os, &chroot_opts) != OS_OK) {
      ylog_error("%s", os_strerror(&os));
      goto end;
    }
  }

  if (io_listen_unix(&io, DAEMON_SOCK) != IO_OK) {
    ylog_error("io_listen_unix: %s", io_strerror(&io));
  } else {
    ylog_info("started");
    accept_loop(&io);
  }

  io_close(&io);
end:
  return EXIT_FAILURE;
}
