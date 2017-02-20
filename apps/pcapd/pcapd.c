#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <pthread.h>

#include <lib/util/ylog.h>
#include <lib/util/os.h>

#define DAEMON_NAME "pcapd"
#define DAEMON_SOCK "pcapd.sock"

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
    fprintf(stderr, "basepath must be an absolute path");
    exit(EXIT_FAILURE);
  } else if (opts->no_daemon == 0 && (opts->gid == 0 || opts->uid == 0)) {
    fprintf(stderr, "daemon must run as unprivileged user:group\n");
    exit(EXIT_FAILURE);
  }
  return 0;
}

#define PCAPCLIF_STARTED (1 << 0)

typedef struct pcapcli_t {
  int flags;
  int fd;
  int id;
  pthread_t tid;
} pcapcli_t;

static pcapcli_t *pcapcli_new(int fd, int id) {
  pcapcli_t *cli;

  if ((cli = malloc(sizeof(pcapcli_t))) == NULL) {
    return NULL;
  }
  cli->flags = 0;
  cli->fd = fd;
  cli->id = id;
  return cli;
}

static void pcapcli_free(pcapcli_t *cli) {
  if (cli != NULL) {
    if (cli->fd >= 0) {
      close(cli->fd);
    }
  }
}

static void *pcapcli_main(void *arg) {
  pcapcli_t *cli = arg;

  ylog_info("pcap client finished (id:%d)", cli->id);
  pcapcli_free(cli);
  return NULL;
}

static int pcapcli_start(pcapcli_t *cli) {
  if (cli->flags & PCAPCLIF_STARTED) {
    errno = EACCES;
    return -1;
  }
  if (pthread_create(&cli->tid, NULL, pcapcli_main, cli) != 0) {
    return -1;
  }
  cli->flags |= PCAPCLIF_STARTED;
  return 0;
}

static void accept_loop(int listenfd) {
  int id_counter = 0;
  pcapcli_t *cli;
  int clifd;
  while(1) {
    if ((clifd = accept(listenfd, NULL, NULL)) < 0) {
      if (errno == EINTR) {
        continue;
      } else {
        break;
      }
    }

    if ((cli = pcapcli_new(clifd, id_counter)) == NULL) {
      ylog_perror("pcapcli_new");
    } else if (pcapcli_start(cli) < 0) {
      ylog_perror("pcapcli_start");
    } else {
      id_counter++;
      ylog_info("pcap client started (id:%d)", cli->id);
    }
  }
  ylog_perror("accept");
}

int main(int argc, char *argv[]) {
  struct sockaddr_un saddr;
  struct pcapd_opts opts;
  os_t os;
  int fd = -1;

  parse_args_or_die(&opts, argc, argv);
  if (opts.no_daemon) {
    ylog_init(DAEMON_NAME, YLOG_STDERR);
    if (chdir(opts.basepath) < 0) {
      ylog_perror("chdir");
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

  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    ylog_perror("socket");
    goto end;
  }
  saddr.sun_family = AF_UNIX;
  snprintf(saddr.sun_path, sizeof(saddr.sun_path), "%s", DAEMON_SOCK);
  unlink(saddr.sun_path);
  if (bind(fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
    ylog_perror("bind");
    goto end;
  } else if (listen(fd, SOMAXCONN) < 0) {
    ylog_perror("listen");
    goto end;
  }

  ylog_info("started");
  accept_loop(fd);
end:
  if (fd >= 0) {
    close(fd);
  }
  return EXIT_FAILURE;
}
