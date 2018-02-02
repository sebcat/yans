#ifndef NET_IFACE_H__
#define NET_IFACE_H__

#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>

#define IFACE_NAMESZ 16
#define IFACE_ADDRSZ 6

#define IFACE_UP       IFF_UP
#define IFACE_LOOPBACK IFF_LOOPBACK

struct iface_srcaddr {
  char ifname[IFACE_NAMESZ];
  union {
    struct sockaddr sa;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
    struct sockaddr_storage st;
  } u;
};

struct iface_entry {
  unsigned int flags;
  int index;
  char name[IFACE_NAMESZ];
  char addr[IFACE_ADDRSZ];
};

struct iface_entries {
  struct iface_entry *ifaces;
  struct iface_srcaddr *ip4srcs;
  struct iface_srcaddr *ip6srcs;
  size_t nifaces;
  size_t nip4srcs;
  size_t nip6srcs;
  int err;
};

int iface_init(struct iface_entries *ifs);
void iface_cleanup(struct iface_entries *ifs);
const char *iface_strerror(struct iface_entries *ifs);

#endif /* NET_IFACE_H__ */

