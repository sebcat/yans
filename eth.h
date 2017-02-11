#ifndef ETH_H_
#define ETH_H_

/* AF_* type for link layer address */
#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if_dl.h>
#elif defined(__linux__)
#error "TODO: Implement Linux support"
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

int eth_valid(const struct sockaddr *saddr);
int eth_init(struct eth_addr *eth, const struct sockaddr *saddr);
int eth_tostring(const struct eth_addr *eth, char *s, size_t len);


#endif
