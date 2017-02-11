#include <string.h>
#include <stdio.h>

#include "eth.h"

#if defined(__FreeBSD__)

int eth_valid(const struct sockaddr *saddr) {
  const struct sockaddr_dl *dl;
  if (saddr->sa_family == AF_LINK) {
    dl = (const struct sockaddr_dl*)saddr;
    if (dl->sdl_alen == ETH_ALEN) {
      return ETHERR_OK;
    }
  }
  return ETHERR_INVALID_IF;
}

int eth_init(struct eth_addr *eth, const struct sockaddr *saddr) {
  const struct sockaddr_dl *dl;
  int ret;

  if ((ret = eth_valid(saddr)) != ETHERR_OK) {
    return ret;
  }
  dl = (struct sockaddr_dl*)saddr;
  eth->index = (int)dl->sdl_index;
  memcpy(eth->addr, LLADDR(dl), ETH_ALEN);
  return ETHERR_OK;
}

#elif defined(__linux__)

int eth_valid(const struct sockaddr *saddr) {
  const struct sockaddr_ll *ll;
  if (saddr->sa_family == AF_PACKET) {
    ll = (const struct sockaddr_ll*)saddr;
    if (ll->sll_halen == ETH_ALEN) {
      return ETHERR_OK;
    }
  }
  return ETHERR_INVALID_IF;
}

int eth_init(struct eth_addr *eth, const struct sockaddr *saddr) {
  const struct sockaddr_ll *ll;
  int ret;

  if ((ret = eth_valid(saddr)) != ETHERR_OK) {
    return ret;
  }
  ll = (struct sockaddr_ll*)saddr;
  eth->index = ll->sll_ifindex;
  memcpy(eth->addr, ll->sll_addr, ETH_ALEN);
  return ETHERR_OK;
}

#endif /* defined(__FreeBSD__) || defined(__linux__) */

int eth_tostring(const struct eth_addr *eth, char *s, size_t len) {
  return snprintf(s, len, "%02x:%02x:%02x:%02x:%02x:%02x",
      (unsigned int)eth->addr[0],
      (unsigned int)eth->addr[1],
      (unsigned int)eth->addr[2],
      (unsigned int)eth->addr[3],
      (unsigned int)eth->addr[4],
      (unsigned int)eth->addr[5]);
}
