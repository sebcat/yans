/* vim: set tabstop=2 shiftwidth=2 expandtab ai: */
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

#include <event2/event.h>
#include <pcap/pcap.h>

#include <lib/util/os.h>
#include <lib/util/io.h>
#include <lib/util/ylog.h>
#include <lib/util/netstring.h>

#define DAEMON_NAME "pcapd"
#define DAEMON_SOCK "pcapd.sock"
#define MAX_ACCEPT_RETRIES 3
#define READCMD_TIMEO_S 5 /* timeout for reading iface name, filter */
#define CMDBUFINITSZ 1024
#define MAX_CMDSZ (1 << 20)
#define SNAPLEN 2048
#define PCAP_TO_MS 1000
#define PCAP_DISPATCH_CNT 64

struct listener {
  io_t io;
  struct event_base *base;
  struct event *lev;
  int aretries;
  unsigned int id_counter;
};

struct client {
  io_t io;
  unsigned int id;
  FILE *dumpf;
  buf_t cmdbuf;
  struct listener *listener;
  struct event *toevent;
  struct event *revent;
  struct event *pcapevent;
  pcap_t *pcap;
  pcap_dumper_t *dumper;
};

void client_close(struct client *cli) {
  io_close(&cli->io);
  buf_cleanup(&cli->cmdbuf);
  if (cli->dumpf != NULL) {
    fclose(cli->dumpf);
  }
  if (cli->toevent != NULL) {
    evtimer_del(cli->toevent);
    event_free(cli->toevent);
  }
  if (cli->revent != NULL) {
    event_del(cli->revent);
    event_free(cli->revent);
  }
  if (cli->pcapevent != NULL) {
    event_del(cli->pcapevent);
    event_free(cli->pcapevent);
  }
  if (cli->pcap != NULL) {
    pcap_close(cli->pcap);
  }
  if (cli->dumper != NULL) {
    pcap_dump_close(cli->dumper);
  }
  free(cli);
}

static void do_recvpcap(int fd, short ev, void *arg) {
  struct client *cli = arg;
  if (pcap_dispatch(cli->pcap, PCAP_DISPATCH_CNT, pcap_dump,
      (u_char*)cli->dumper) < 0) {
    ylog_error("pcapcli%u: pcap_dispatch: %s", cli->id,
        pcap_geterr(cli->pcap));
    client_close(cli);
  }
}

static void do_recvclose(int fd, short ev, void *arg) {
  struct client *cli = arg;
  ylog_info("pcapcli%u: ended", cli->id);
  client_close(cli);
}

static void do_recvcmd(int fd, short ev, void *arg) {
  struct client *cli = arg;
  char *iface;
  char *filter = NULL;
  char *end;
  char *curr;
  size_t filtersz;
  int ret;
  int pcapfd;
  char errbuf[PCAP_ERRBUF_SIZE];
  struct bpf_program bpf;

  if (io_readbuf(&cli->io, &cli->cmdbuf, NULL) != IO_OK) {
    ylog_error("pcapcli%u: io_readbuf: %s", cli->id, io_strerror(&cli->io));
    goto fail;
  }

  ret = netstring_parse(&iface, NULL, cli->cmdbuf.data, cli->cmdbuf.len);
  if (ret != NETSTRING_OK) {
    if (ret == NETSTRING_ERRINCOMPLETE) {
      if (cli->cmdbuf.len >= MAX_CMDSZ) {
        ylog_error("pcapcli%u: maximum command size exceeded", cli->id);
        client_close(cli);
      }
      return;
    } else {
      ylog_error("pcapcli%u: netstring_parse: %s", cli->id,
          netstring_strerror(ret));
      client_close(cli);
      return;
    }
  }

  for(curr = iface, end = cli->cmdbuf.data + cli->cmdbuf.len - 1;
      curr < end;
      curr++) {
    if (*curr == '\0') {
      filter = curr+1;
      break;
    }
  }

  if (filter == NULL) {
    ylog_error("pcapcli%u: malformed command string", cli->id);
    goto fail;
  }

  filtersz = strlen(filter);
  ylog_info("pcapcli%u: iface:\"%s\" filterlen:\"%zu\"", cli->id, iface,
      strlen(filter));

  errbuf[0] = '\0';
  if ((cli->pcap = pcap_open_live(iface, SNAPLEN, 0, PCAP_TO_MS,
      errbuf)) == NULL) {
    ylog_error("pcapcli%u: pcap_open_live: %s", cli->id, errbuf);
    goto fail;
  } else if (errbuf[0] != '\0') {
    ylog_info("pcapcli%u: pcap_open_live warning: %s", cli->id, errbuf);
  }

  if (filtersz > 0) {
    if (pcap_compile(cli->pcap, &bpf, filter, 1, PCAP_NETMASK_UNKNOWN) < 0) {
      ylog_error("pcapcli%u: pcap_compile: %s", cli->id,
          pcap_geterr(cli->pcap));
      goto fail;
    }
    ret = pcap_setfilter(cli->pcap, &bpf);
    pcap_freecode(&bpf);
    if (ret < 0) {
      ylog_error("pcapcli%u: pcap_setfilter: %s", cli->id,
          pcap_geterr(cli->pcap));
      goto fail;
    }
  }

  if ((cli->dumper = pcap_dump_fopen(cli->pcap, cli->dumpf)) == NULL) {
    ylog_error("pcapcli%u: pcap_dump_fopen: %s", cli->id,
        pcap_geterr(cli->pcap));
    goto fail;
  }
  cli->dumpf = NULL; /* cli->dumper has ownership over cli->dumpf, and will
                      * clear it on pcap_dump_close */
  if (pcap_setnonblock(cli->pcap, 1, errbuf) < 0) {
    ylog_error("pcapcli%u: pcap_setnonblock: %s", cli->id, errbuf);
    goto fail;
  }

  event_del(cli->toevent);
  event_free(cli->toevent);
  cli->toevent = NULL;

  event_del(cli->revent);
  event_free(cli->revent);
  cli->revent = event_new(cli->listener->base, IO_FILENO(&cli->io),
      EV_READ|EV_PERSIST, do_recvclose, cli);
  if (cli->revent == NULL) {
    ylog_perror("do_recvfd event_new");
    goto fail;
  }

  if ((pcapfd = pcap_get_selectable_fd(cli->pcap)) < 0) {
    ylog_error("pcapcli%u: pcap_get_selectable_fd failure", cli->id);
    goto fail;
  }

  cli->pcapevent = event_new(cli->listener->base, pcapfd, EV_READ|EV_PERSIST,
      do_recvpcap, cli);
  if (cli->pcapevent == NULL) {
    ylog_perror("do_recvfd event_new pcapevent");
    goto fail;
  }

  event_add(cli->revent, NULL);
  event_add(cli->pcapevent, NULL);


  return;
fail:
  client_close(cli);
  return;
}

static void do_recvfd(int fd, short ev, void *arg) {
  struct client *cli = arg;
  int rfd;
  FILE *fp;
  if (io_recvfd(&cli->io, &rfd) != IO_OK) {
    ylog_error("pcapcli%u: io_recvfd: %s", cli->id, io_strerror(&cli->io));
    client_close(cli);
    return;
  }

  if ((fp = fdopen(rfd, "w")) == NULL) {
    ylog_error("pcapcli%u: fdopen: %s", cli->id, io_strerror(&cli->io));
    client_close(cli);
    return;
  }
  cli->dumpf = fp;

  event_del(cli->revent);
  event_free(cli->revent);
  cli->revent = event_new(cli->listener->base, IO_FILENO(&cli->io),
      EV_READ|EV_PERSIST, do_recvcmd, cli);
  if (cli->revent == NULL) {
    ylog_perror("do_recvfd event_new");
    client_close(cli);
    return;
  }
  event_add(cli->revent, NULL);
}

static void do_readto(int fd, short ev, void *arg) {
  struct client *cli = arg;
  ylog_error("pcapcli%u: read timeout", cli->id);
  client_close(cli);
}

static void do_accept(int fd, short ev, void *arg) {
  struct listener *listener = arg;
  struct client *cli;
  struct timeval tv;
  io_t client;

  if (io_accept(&listener->io, &client) != IO_OK) {
    listener->aretries++;
    ylog_error("io_accept: %s", io_strerror(&listener->io));
    if (listener->aretries >= MAX_ACCEPT_RETRIES) {
      event_base_loopexit(listener->base, NULL);
    }
    return;
  }

  if ((cli = malloc(sizeof(struct client))) == NULL) {
    ylog_perror("do_accept malloc");
    io_close(&client);
    return;
  }
  IO_INIT(&cli->io, IO_FILENO(&client));
  cli->dumpf = NULL;
  cli->pcap = NULL;
  cli->dumper = NULL;
  cli->pcapevent = NULL;
  cli->listener = listener;
  cli->id = listener->id_counter++;

  cli->toevent = evtimer_new(listener->base, do_readto, cli);
  if (cli->toevent == NULL) {
    ylog_perror("do_accept evtimer_new");
    io_close(&client);
    free(cli);
    return;
  }
  cli->revent = event_new(listener->base, IO_FILENO(&client),
      EV_READ|EV_PERSIST, do_recvfd, cli);
  if (cli->revent == NULL) {
    ylog_perror("do_accept event_new");
    event_free(cli->toevent);
    io_close(&client);
    free(cli);
    return;
  }

  tv.tv_sec = READCMD_TIMEO_S;
  tv.tv_usec = 0;
  evtimer_add(cli->toevent, &tv);
  event_add(cli->revent, NULL);
  if (buf_init(&cli->cmdbuf, CMDBUFINITSZ) == NULL) {
    client_close(cli);
  }

  listener->aretries = 0;
  ylog_info("pcapcli%u: connected", cli->id);
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
  struct listener listener;
  struct pcapd_opts opts;
  os_t os;

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

  if (io_listen_unix(&listener.io, DAEMON_SOCK) != IO_OK) {
    ylog_error("io_listen_unix: %s", io_strerror(&listener.io));
    goto end;
  }

  io_setnonblock(&listener.io, 1);
  listener.aretries = 0;
  listener.id_counter = 0;
  listener.base = event_base_new();
  listener.lev = event_new(listener.base, IO_FILENO(&listener.io),
      EV_READ|EV_PERSIST, do_accept, &listener);
  if (listener.lev == NULL) {
    ylog_error("listener event_new failure");
    io_close(&listener.io);
    goto end;
  }

  event_add(listener.lev, NULL);
  ylog_info("started");
  event_base_dispatch(listener.base);
  ylog_error("accept: maximum number of retries reached");
  event_free(listener.lev);
  event_base_free(listener.base);
  io_close(&listener.io);
end:
  return EXIT_FAILURE;
}
