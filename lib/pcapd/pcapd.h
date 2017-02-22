/* pcapd IPC library */
#ifndef PCAPD_H_
#define PCAPD_H_

#include <lib/util/io.h>

#define PCAPD_ERRBUFSZ    52

#define PCAPD_OK   0
#define PCAPD_ERR -1

#define pcapd_setup(pcapd) \
  do { \
    (pcapd)->fd = -1; \
    (pcapd)->errbuf[0] = '\0'; \
  } while(0);

typedef struct pcapd_t {
  int fd;
  char errbuf[PCAPD_ERRBUFSZ];
} pcapd_t;

const char *pcapd_strerror(pcapd_t *pcapd);
int pcapd_listen(pcapd_t *pcapd, const char *path);
int pcapd_connect(pcapd_t *pcapd, const char *path);
int pcapd_accept(pcapd_t *pcapd, pcapd_t *cli);
int pcapd_wropen(pcapd_t *pcapd, const char *iface, const char *filter);
int pcapd_close(pcapd_t *pcapd);

#endif
