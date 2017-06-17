#ifndef ETHD_PCAP_H
#define ETHD_PCAP_H

#include <stdio.h>

#include <pcap/pcap.h>

#include <lib/util/buf.h>
#include <lib/util/eds.h>

#include <proto/pcap.h>

/* defined here so the main application knows it's size, but shouldn't be
 * used externally from pcap.c */
struct pcap_client {
  FILE *dumpf;
  buf_t cmdbuf;
  struct p_pcap_cmd cmd;
  pcap_t *pcap;
  pcap_dumper_t *dumper;
  struct eds_client *dumpcli;
};

eds_action_result pcap_on_readable(struct eds_client *cli, int fd);
void pcap_on_done(struct eds_client *cli, int fd);

#endif
