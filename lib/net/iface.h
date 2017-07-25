#ifndef NET_IFACE_H__
#define NET_IFACE_H__

#include <ifaddrs.h>

struct iface {
  struct ifaddrs *addrs;
  struct ifaddrs *curr;
  int err;
};

int iface_init(struct iface *iface);
const char *iface_next(struct iface *iface);
void iface_cleanup(struct iface *iface);
const char *iface_strerror(struct iface *iface);

#endif /* NET_IFACE_H__ */

