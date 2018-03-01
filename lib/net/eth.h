#ifndef ETH_H_
#define ETH_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* AF_* type for link layer address */
#if defined(__FreeBSD__)
#include <net/if_dl.h>
#elif defined(__linux__)
#include <linux/if_packet.h>
#else
#error "Unsupported platform"
#endif

#if !defined(ETH_ALEN)
#define ETH_ALEN 6
#endif

#define ETH_STRSZ (ETH_ALEN*3)

#define ETHERR_OK            0
#define ETHERR_INVALID_IF   -1

struct eth_sender {
  /* -- internal fields -- */
  int fd;
  int index; /* XXX: platform dependent */
  int lasterr;
};

int eth_addr_tostring(const char *eth, char *s, size_t len);
int eth_addr_parse(const char *eth, char *dst, size_t dstlen);

int eth_sender_init(struct eth_sender *eth, const char *iface);
void eth_sender_cleanup(struct eth_sender *eth);
ssize_t eth_sender_write(struct eth_sender *eth, const void *data, size_t len);
const char *eth_sender_strerror(struct eth_sender *eth);

#endif
