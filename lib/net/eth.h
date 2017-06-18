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

struct eth_addr {
  int index;
  unsigned char addr[ETH_ALEN];
};

#define ETHERR_OK            0
#define ETHERR_INVALID_IF   -1

struct eth_sender {
  int fd;
  int index;
};

int eth_addr_valid(const struct sockaddr *saddr);
int eth_addr_init(struct eth_addr *eth, const struct sockaddr *saddr);
void eth_addr_init_bytes(struct eth_addr *eth, const char *data);
int eth_addr_tostring(const struct eth_addr *eth, char *s, size_t len);

int eth_sender_init(struct eth_sender *eth, const char *iface);
void eth_sender_cleanup(struct eth_sender *eth);
ssize_t eth_sender_write(struct eth_sender *eth, void *data, size_t len);

#endif
