/* vim: set tabstop=2 shiftwidth=2 expandtab ai: */
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>

#ifdef __linux__
#include <sys/prctl.h>
#include <sys/capability.h>
#endif

#include <pcap/pcap.h>

#include <lib/util/os.h>
#include <lib/util/fio.h>
#include <lib/util/io.h>
#include <lib/util/ylog.h>

#define DAEMON_NAME "pcapd"
#define DAEMON_SOCK "pcapd.sock"
#define MAX_ACCEPT_RETRIES 3
#define MAX_FILTERSZ (1 << 20)
#define SNAPLEN 2048
#define PCAP_TO_MS 1000
#define POLL_TO_MS -1
#define PCAP_DISPATCH_CNT 64
#define PCAPD_STACKSZ (1 << 16)

/* represents a connected client session */
typedef struct pcapcli_t {
  int id;
  pthread_t tid;
  FILE *fp;
} pcapcli_t;

static void pcapcli_free(pcapcli_t *cli) {
  if (cli != NULL) {
    if (cli->fp != NULL) {
      fclose(cli->fp);
      cli->fp = NULL;
    }
  }
}

static void pcapcli_loop(pcapcli_t *cli, pcap_t *pcap, pcap_dumper_t *dumper) {
  struct pollfd fds[2];
  int clifd = fileno(cli->fp);
  char closebuf[16];
  int ret;
  int pcapfd;

  if ((pcapfd = pcap_get_selectable_fd(pcap)) < 0) {
    ylog_error("pcapcli%d: pcap_get_selectable_fd failure", cli->id);
    return;
  }

  for(;;) {
    fds[0].fd = clifd;
    fds[0].events = POLLIN | POLLRDBAND;
    fds[1].fd = pcapfd;
    fds[1].events = POLLIN | POLLRDBAND;
    do {
      ret = poll(fds, sizeof(fds) / sizeof(struct pollfd), POLL_TO_MS);
    } while (ret < 0 && errno == EINTR);
    if (ret == 0) {
      continue;
    } else if (ret < 0) {
      ylog_error("pcapcli%d: poll: %s", cli->id, strerror(errno));
      break;
    }

    /* check clifd */
    if (fds[0].revents & (POLLIN | POLLRDBAND)) {
      /* clifd has data, this indicates that clifd wants to close the
       * listener gracefully. It should be an empty netstring, but we
       * don't really care if it is. It should be though, because otherwise
       * we may end up blocking on a read. We want to close gracefully,
       * because when we close the client fd, we signal that we're done
       * with the dumpfile. If the client disconnects and we're in a dumpfile
       * write, the client might do things with the dumpfile while pcapd is
       * writing to it, which is less-than-ideal. */
      fio_readns(cli->fp, closebuf, sizeof(closebuf));
      ylog_info("pcapcli%d: client disconnected gracefully", cli->id);
      return;
    } else if (fds[0].revents & (POLLHUP | POLLERR)) {
      ylog_error("pcapcli%d: client disconnected prematurely", cli->id);
      return;
    }

    /* check pcapfd */
    if (fds[1].revents & (POLLIN | POLLRDBAND)) {
      if (pcap_dispatch(pcap, PCAP_DISPATCH_CNT, pcap_dump, (u_char*)dumper)
          < 0) {
        ylog_error("pcapcli%d: pcap_dispatch: %s", cli->id, pcap_geterr(pcap));
        return;
      }
    } else if (fds[1].revents & (POLLHUP | POLLERR)) {
      ylog_error("pcapcli%d: pcapfd closed unexpectedly", cli->id);
      return;
    }
  }
}

static void *pcapcli_main(void *arg) {
  pcapcli_t *cli = arg;
  char iface[64];
  char *filter = NULL;
  int dumpfd = -1;
  int ret;
  io_t io;
  pcap_t *pcap = NULL;
  char errbuf[PCAP_ERRBUF_SIZE];
  size_t filtersz;
  FILE *dumpf = NULL;
  struct bpf_program bpf;
  pcap_dumper_t *dumper = NULL;

  /* borrow the fd from cli->fp. Closing &c is still managed by
   * pcapcli_free. mixing io_* with FILE* I/O on same underlying
   * fd should be done carefully, as it is mixing buffered
   * with unbuffered code. */
  io_init(&io, fileno(cli->fp));
  if (io_recvfd(&io, &dumpfd) != IO_OK) {
    ylog_error("pcapcli%d: io_recvfd: %s", cli->id, io_strerror(&io));
    goto end;
  } else if ((dumpf = fdopen(dumpfd, "w")) == NULL) {
    ylog_error("pcapcli%d: fdopen: %s", cli->id, strerror(errno));
    goto end;
  }
  dumpfd = -1; /* dumpf takes ownership of dumpfd */

  if ((ret = fio_readns(cli->fp, iface, sizeof(iface))) != FIO_OK) {
    ylog_error("pcapcli%d: iface: %s", cli->id, fio_strerror(ret));
    goto end;
  }

  if ((ret = fio_readnsa(cli->fp, MAX_FILTERSZ, &filter, &filtersz)) !=
      FIO_OK) {
    ylog_error("pcapcli%d: filter: %s", cli->id, fio_strerror(ret));
  }

  ylog_info("pcapcli%d: starting iface:\"%s\" filtersz:%zuB",
      cli->id, iface, filtersz);

  errbuf[0] = '\0';
  if ((pcap = pcap_open_live(iface, SNAPLEN, 0, PCAP_TO_MS, errbuf)) == NULL) {
    ylog_error("pcapcli%d: pcap_open_live: %s", cli->id, errbuf);
    goto end;
  } else if (errbuf[0] != '\0') {
    ylog_info("pcapcli%d: pcap_open_live warning: %s", cli->id, errbuf);
  }

  if (filtersz > 0) {
    if (pcap_compile(pcap, &bpf, filter, 1, PCAP_NETMASK_UNKNOWN) < 0) {
      ylog_error("pcapcli%d: pcap_compile: %s", cli->id, pcap_geterr(pcap));
      goto end;
    }
    ret = pcap_setfilter(pcap, &bpf);
    pcap_freecode(&bpf);
    if (ret < 0) {
      ylog_error("pcapcli%d: pcap_setfilter: %s", cli->id, pcap_geterr(pcap));
      goto end;
    }
  }

  if ((dumper = pcap_dump_fopen(pcap, dumpf)) == NULL) {
    ylog_error("pcapcli%d: pcap_dump_fopen: %s", cli->id, pcap_geterr(pcap));
    goto end;
  }
  dumpf = NULL; /* dumper has ownership over dumpf, and will clear it on
                 * pcap_dump_close */

  if (pcap_setnonblock(pcap, 1, errbuf) < 0) {
    ylog_error("pcapcli%d: pcap_setnonblock: %s", cli->id, errbuf);
    goto end;
  }

  pcapcli_loop(cli, pcap, dumper);

end:
  ylog_info("pcapcli%d: ended", cli->id);
  if (dumper != NULL) {
    pcap_dump_close(dumper);
  }
  if (dumpfd >= 0) {
    close(dumpfd);
  }
  if (dumpf != NULL) {
    fclose(dumpf);
  }
  if (filter != NULL) {
    free(filter);
  }
  if (pcap != NULL) {
    pcap_close(pcap);
  }
  pcapcli_free(cli);
  return NULL;
}

static pcapcli_t *pcapcli_new(FILE *fp, int id) {
  pthread_attr_t attrs;
  pcapcli_t *cli;
  int ret;

  if ((ret = pthread_attr_init(&attrs)) != 0) {
    ylog_error("pcapcli%d: pthread_attr_init: %s", id, strerror(ret));
    return NULL;
  }

  /* create the thread in a detached state, so that the thread resources are
   * released on thread completion and not on explicit join */
  if ((ret = pthread_attr_setdetachstate(&attrs,
      PTHREAD_CREATE_DETACHED)) != 0) {
    ylog_error("pcapcli%d: pthread_attr_setdetachstate: %s", id,
        strerror(ret));
    pthread_attr_destroy(&attrs);
    return NULL;
  }

  /* set a predictable, relatively low stack size */
  if ((ret = pthread_attr_setstacksize(&attrs, PCAPD_STACKSZ)) != 0) {
    ylog_error("pcapcli%d: pthread_attr_setstacksize: %s", id,
        strerror(ret));
    pthread_attr_destroy(&attrs);
    return NULL;
  }

  if ((cli = malloc(sizeof(pcapcli_t))) == NULL) {
    ylog_error("pcapcli%d: malloc: %s", id, strerror(errno));
    pthread_attr_destroy(&attrs);
    return NULL;
  }
  cli->fp = fp;
  cli->id = id;
  ret = pthread_create(&cli->tid, &attrs, pcapcli_main, cli);
  pthread_attr_destroy(&attrs);
  if (ret != 0) {
    free(cli);
    ylog_error("pcapcli%d: pthread_create: %s", id, strerror(ret));
    return NULL;
  }
  return cli;
}

static void accept_loop(io_t *listener) {
  int id_counter = 0, retries = 0;
  io_t client;
  FILE *fp;
  while(1) {
    if (io_accept(listener, &client) != IO_OK) {
      retries++;
      ylog_error("io_accept: %s", io_strerror(listener));
      if (retries >= MAX_ACCEPT_RETRIES) {
        break;
      }
      continue;
    }

    retries = 0;
    if (io_tofp(&client, "w+", &fp) != IO_OK) {
      ylog_error("io_tofp: %s", io_strerror(&client));
      io_close(&client);
      continue;
    }
    if (pcapcli_new(fp, id_counter) == NULL) {
      /* pcapcli_new does logging of errors */
      fclose(fp);
    } else {
      ylog_info("pcapcli%d: connected", id_counter);
    }
    id_counter++;
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

  signal(SIGPIPE, SIG_IGN);
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
      cap_set_flag(caps, CAP_PERMITTED, sizeof(newcaps)/sizeof(cap_value_t),
          newcaps, CAP_SET);
      cap_set_flag(caps, CAP_EFFECTIVE, sizeof(newcaps)/sizeof(cap_value_t),
          newcaps, CAP_SET);
      cap_set_flag(caps, CAP_INHERITABLE, sizeof(newcaps)/sizeof(cap_value_t),
          newcaps, CAP_SET);
      if (cap_set_proc(caps) < 0) {
        cap_free(caps);
        ylog_error("cap_set_proc pre-chroot: %s", strerror(errno));
        goto end;
      }
      cap_free(caps);
    } while(0);
#endif

    chroot_opts.name = DAEMON_NAME;
    chroot_opts.path = opts.basepath;
    chroot_opts.uid = opts.uid;
    chroot_opts.gid = opts.gid;
    chroot_opts.nagroups = 0;
    if (os_chrootd(&os, &chroot_opts) != OS_OK) {
      ylog_error("%s", os_strerror(&os));
      goto end;
    }
    /* For Linux: After chrooting, drop the capabilities that was needed to
     * establish the chroot from the permitted set while keeping CAP_NET_RAW.
     * Set CAP_NET_RAW in the effecive set which was cleared on setuid() */
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
        goto end;
      }
      cap_free(caps);
    } while(0);
#endif
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
