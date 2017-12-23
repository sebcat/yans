#ifndef NET_IFACE_H__
#define NET_IFACE_H__

#include <ifaddrs.h>
#include <net/if.h>

#define IFACE_NAMESZ 16
#define IFACE_ADDRSZ 6

#define IFACE_UP       IFF_UP
#define IFACE_LOOPBACK IFF_LOOPBACK

struct iface_entry {
  unsigned int flags;
  int index;
  char name[IFACE_NAMESZ];
  char addr[IFACE_ADDRSZ];
};

struct iface_entries {
  int err;
  size_t nentries;
  struct iface_entry *entries;
};

int iface_init(struct iface_entries *ifs);
void iface_cleanup(struct iface_entries *ifs);
const char *iface_strerror(struct iface_entries *ifs);

#endif /* NET_IFACE_H__ */

