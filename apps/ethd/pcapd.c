#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <event2/event.h>
#include <pcap/pcap.h>

#include <lib/util/io.h>
#include <lib/util/ylog.h>
#include <lib/util/netstring.h>

#include <apps/ethd/pcapd.h>

#define MAX_ACCEPT_RETRIES 3
#define READCMD_TIMEO_S 5 /* timeout for reading iface name, filter */
#define CMDBUFINITSZ 1024
#define MAX_CMDSZ (1 << 20)
#define SNAPLEN 2048
#define PCAP_TO_MS 1000
#define PCAP_DISPATCH_CNT 64

struct pcapd_listener {
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
  struct pcapd_listener *listener;
  struct event *toevent;
  struct event *revent;
  struct event *pcapevent;
  pcap_t *pcap;
  pcap_dumper_t *dumper;
};

static void client_close(struct client *cli) {
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
  struct pcapd_listener *listener = arg;
  struct client *cli;
  struct timeval tv;
  io_t client;

  printf("%d\n", listener->io.fd);
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

void free_pcapd_listener(pcapd_listener_t *listener) {
  if (listener != NULL) {
    io_close(&listener->io);
    if (listener->lev != NULL) {
      event_free(listener->lev);
    }
    free(listener);
  }
}

pcapd_listener_t *create_pcapd_listener(struct event_base *base, char *path) {
  struct pcapd_listener *listener = NULL;

  if ((listener = malloc(sizeof(struct pcapd_listener))) == NULL) {
    ylog_error("malloc: %s", strerror(errno));
    goto fail;
  }

  memset(listener, 0, sizeof(struct pcapd_listener));

  if (io_listen_unix(&listener->io, path) != IO_OK) {
    ylog_error("io_listen_unix: %s", io_strerror(&listener->io));
    goto fail;
  }

  io_setnonblock(&listener->io, 1);
  listener->base = base;
  listener->lev = event_new(listener->base, IO_FILENO(&listener->io),
      EV_READ|EV_PERSIST, do_accept, listener);
  if (listener->lev == NULL) {
    ylog_error("listener event_new failure");
    io_close(&listener->io);
    goto fail;
  }

  event_add(listener->lev, NULL);
  return listener;

fail:
  if (listener == NULL) {
    if (listener->lev != NULL) {
      event_free(listener->lev);
    }
    free(listener);
  }

  return NULL;
}
