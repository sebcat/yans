#ifndef NET_ROUTE__
#define NET_ROUTE__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <lib/net/ip.h>

/* struct route_table_entry flags */
#define RTENTRY_UP      (1 << 0)
#define RTENTRY_GW      (1 << 1)
#define RTENTRY_HOST    (1 << 2)
#define RTENTRY_REJECT  (1 << 4)
#define RTENTRY_STATIC  (1 << 5)

struct route_table_entry {
  int flags;
  ip_addr_t addr;
  ip_addr_t mask;
  ip_addr_t gw;
  int gw_ifindex; /* negative if gw is an IP address, otherwise this contains
                   * the interface index and gw should not be used */
  int ifindex;
};

struct route_table {
  size_t nentries;
  struct route_table_entry *entries;
  char *errprefix;
  int errnum;
};

int route_table_init(struct route_table *rt);
void route_table_cleanup(struct route_table *rt);
void route_table_strerror(struct route_table *rt, char *buf, size_t len);

#endif /* NET_ROUTE__ */
