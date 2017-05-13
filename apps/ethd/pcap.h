#ifndef ETHD_PCAP_H
#define ETHD_PCAP_H

typedef struct pcap_listener pcap_listener_t;

pcap_listener_t *create_pcap_listener(struct event_base *base, char *path);
void free_pcap_listener(pcap_listener_t *listener);

#endif
