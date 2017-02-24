/* pcapd IPC library */
#ifndef PCAPD_H_
#define PCAPD_H_

#include <lib/util/buf.h>

#define PCAPD_ERRBUFSZ    52

#define PCAPD_OK   0
#define PCAPD_ERR -1

typedef struct pcapd_t {
  int fd;
  buf_t buf;
  char errbuf[PCAPD_ERRBUFSZ];
} pcapd_t;

struct pcapd_openmsg {
  char *iface;
  char *filter;
};

int pcapd_init(pcapd_t *pcapd, size_t bufsz);
const char *pcapd_strerror(pcapd_t *pcapd);
int pcapd_listen(pcapd_t *pcapd, const char *path);
int pcapd_connect(pcapd_t *pcapd, const char *path);
int pcapd_accept(pcapd_t *pcapd, pcapd_t *cli);
int pcapd_rdopen(pcapd_t *pcapd);
int pcapd_wropen(pcapd_t *pcapd, const char *iface, const char *filter);
int pcapd_close(pcapd_t *pcapd);

#endif
