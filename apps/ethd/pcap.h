#ifndef ETHD_PCAP_H
#define ETHD_PCAP_H

#include <stdio.h>

#include <pcap/pcap.h>

#include <lib/util/buf.h>
#include <lib/util/eds.h>

#include <lib/ycl/ycl.h>

#define PCAP_CLIENT(cli__) \
  (struct pcap_client *)((cli__)->udata)

#define PCAP_CLIENT_COMMON(cli__) \
  (struct pcap_client_common*)((cli__)->udata)

#define PCAP_CLIENT_DUMPER(cli__) \
  (struct pcap_client_dumper*)((cli__)->udata)

struct pcap_client_common {
  int flags;
  struct ycl_msg msgbuf;
};

struct pcap_client_dumper {
  struct pcap_client_common common;
  struct eds_client *parent;
};

struct pcap_client {
  struct pcap_client_common common;
  struct ycl_ctx ycl;
  FILE *dumpf;
  pcap_t *pcap;
  pcap_dumper_t *dumper;
  struct eds_client *dumpcli;
  char msg[256];
};

struct pcap_clients {
  union {
    struct pcap_client cli;
    struct pcap_client_dumper dumper;
  } u;
};

void pcap_on_readable(struct eds_client *cli, int fd);
void pcap_on_done(struct eds_client *cli, int fd);
void pcap_on_finalize(struct eds_client *cli);

#endif
