#ifndef ETHD_PCAPD_H
#define ETHD_PCAPD_H

typedef struct pcapd_listener pcapd_listener_t;

pcapd_listener_t *create_pcapd_listener(struct event_base *base, char *path);
void free_pcapd_listener(pcapd_listener_t *listener);

#endif
