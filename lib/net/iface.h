#ifndef NET_IFACE_H__
#define NET_IFACE_H__

#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>

#include <lib/net/ip.h>

#define IFACE_NAMESZ 16
#define IFACE_ADDRSZ 6

#define IFACE_UP       IFF_UP
#define IFACE_LOOPBACK IFF_LOOPBACK

struct iface_srcaddr {
  char ifname[IFACE_NAMESZ];
  ip_addr_t addr;
  ip_addr_t mask;
};

struct iface_entry {
  unsigned int flags;
  int index;
  char name[IFACE_NAMESZ];
  char addr[IFACE_ADDRSZ];
};

struct iface_entries {
  struct iface_entry *ifaces;
  struct iface_srcaddr *ipsrcs;
  size_t nifaces;
  size_t nipsrcs;
  int err;
};

int iface_init(struct iface_entries *ifs);
void iface_cleanup(struct iface_entries *ifs);
const char *iface_strerror(struct iface_entries *ifs);

#endif /* NET_IFACE_H__ */

