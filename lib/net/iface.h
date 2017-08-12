#ifndef NET_IFACE_H__
#define NET_IFACE_H__

#include <ifaddrs.h>
#include <net/if.h>

#define IFACE_NAMESZ 16
#define IFACE_ADDRSZ 6

struct iface {
  struct ifaddrs *addrs;
  struct ifaddrs *curr;
  int err;
};

#define IFACE_UP       IFF_UP
#define IFACE_LOOPBACK IFF_LOOPBACK

struct iface_entry {
  unsigned int flags;
  int index;
  char name[IFACE_NAMESZ];
  char addr[IFACE_ADDRSZ];
};

int iface_init(struct iface *iface);
int iface_next(struct iface *iface, struct iface_entry *out);
void iface_cleanup(struct iface *iface);
const char *iface_strerror(struct iface *iface);

#endif /* NET_IFACE_H__ */

