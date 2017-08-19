#ifndef NET_ROUTE__
#define NET_ROUTE__

#include <netinet/in.h>

#define ROUTE_TABLE_ERRBUFSZ 128

typedef union {
  struct sockaddr sa;
  struct sockaddr_storage st;
  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;
} route_table_addr_t;

#define RTENTRY_UP      (1 << 0)
#define RTENTRY_GW      (1 << 1)
#define RTENTRY_HOST    (1 << 2)
#define RTENTRY_REJECT  (1 << 4)
#define RTENTRY_STATIC  (1 << 5)

struct route_table_entry {
  int flags;
  route_table_addr_t addr;
  route_table_addr_t mask;
  route_table_addr_t gw;
  int gw_ifindex; /* negative if gw is an IP address, otherwise this contains
                   * the interface index and gw should not be used
                   * (platform dependent) */
  int ifindex;
};

struct route_table_entries {
  char *buf;
  size_t off;
  size_t len;
};

struct route_table {
  struct route_table_entries ip4;
  struct route_table_entries ip6;
  char *errprefix;
  int errnum;
};

int route_table_init(struct route_table *rt);
void route_table_cleanup(struct route_table *rt);
void route_table_strerror(struct route_table *rt, char *buf, size_t len);

int route_table_next_ip4(struct route_table *rt,
    struct route_table_entry *out);
int route_table_next_ip6(struct route_table *rt,
    struct route_table_entry *out);

#endif /* NET_ROUTE__ */
