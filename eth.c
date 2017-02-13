#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#if defined(__FreeBSD__)
#include <net/if.h>
#include <net/bpf.h>
#endif /* defined(__FreeBSD__) */

#include "eth.h"

struct eth_sender_t {
  int fd;
};

#if defined(__FreeBSD__)

int eth_addr_valid(const struct sockaddr *saddr) {
  const struct sockaddr_dl *dl;
  if (saddr->sa_family == AF_LINK) {
    dl = (const struct sockaddr_dl*)saddr;
    if (dl->sdl_alen == ETH_ALEN) {
      return ETHERR_OK;
    }
  }
  return ETHERR_INVALID_IF;
}

int eth_addr_init(struct eth_addr *eth, const struct sockaddr *saddr) {
  const struct sockaddr_dl *dl;
  int ret;

  if ((ret = eth_addr_valid(saddr)) != ETHERR_OK) {
    return ret;
  }
  dl = (struct sockaddr_dl*)saddr;
  eth->index = (int)dl->sdl_index;
  memcpy(eth->addr, LLADDR(dl), ETH_ALEN);
  return ETHERR_OK;
}

eth_sender_t *eth_sender_new(const char *iface) {
  int fd = -1;
  struct ifreq ifr;
  eth_sender_t *sender = NULL;

  if ((sender = calloc(1, sizeof(eth_sender_t))) == NULL) {
    goto fail;
  }

  if (( fd = open("/dev/bpf", O_WRONLY)) < 0) {
    goto fail;
  }

  snprintf(ifr.ifr_name, IFNAMSIZ, "%s", iface);
  if (ioctl(fd, BIOCSETIF, &ifr)  == -1) {
    goto fail;
  }

  sender->fd = fd;
  return sender;

fail:
  if (sender != NULL) {
    free(sender);
  }
  if (fd != -1) {
    close(fd);
  }
  return NULL;
}

void eth_sender_free(eth_sender_t *sender) {
  if (sender != NULL) {
    close(sender->fd);
    free(sender);
  }
}

ssize_t eth_sender_send(eth_sender_t *sender, void *data, size_t len) {
  ssize_t ret;
  do {
    ret = write(sender->fd, data, len);
  } while(ret == -1 && errno == EINTR);
  return ret;
}


#elif defined(__linux__)

int eth_addr_valid(const struct sockaddr *saddr) {
  const struct sockaddr_ll *ll;
  if (saddr->sa_family == AF_PACKET) {
    ll = (const struct sockaddr_ll*)saddr;
    if (ll->sll_halen == ETH_ALEN) {
      return ETHERR_OK;
    }
  }
  return ETHERR_INVALID_IF;
}

int eth_addr_init(struct eth_addr *eth, const struct sockaddr *saddr) {
  const struct sockaddr_ll *ll;
  int ret;

  if ((ret = eth_addr_valid(saddr)) != ETHERR_OK) {
    return ret;
  }
  ll = (struct sockaddr_ll*)saddr;
  eth->index = ll->sll_ifindex;
  memcpy(eth->addr, ll->sll_addr, ETH_ALEN);
  return ETHERR_OK;
}

#endif /* defined(__FreeBSD__) || defined(__linux__) */

int eth_addr_tostring(const struct eth_addr *eth, char *s, size_t len) {
  return snprintf(s, len, "%02x:%02x:%02x:%02x:%02x:%02x",
      (unsigned int)eth->addr[0],
      (unsigned int)eth->addr[1],
      (unsigned int)eth->addr[2],
      (unsigned int)eth->addr[3],
      (unsigned int)eth->addr[4],
      (unsigned int)eth->addr[5]);
}
