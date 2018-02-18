#ifndef YANS_NEIGH_H__
#define YANS_NEIGH_H__

#include <stddef.h>
#include <netinet/in.h>

#define NEIGH_IFNAMSIZ 16 /* including trailing \0 */
#define NEIGH_ETHSIZ 6

struct neigh_ip4_entry {
  struct sockaddr_in sin;
  char iface[NEIGH_IFNAMSIZ];
  char hwaddr[NEIGH_ETHSIZ];
};

struct neigh_ip6_entry {
  struct sockaddr_in6 sin6;
  char iface[NEIGH_IFNAMSIZ];
  char hwaddr[NEIGH_ETHSIZ];
};

struct neigh_ip4_entry *neigh_get_ip4_entries(size_t *nentries, int *err);
struct neigh_ip6_entry *neigh_get_ip6_entries(size_t *nentries, int *err);
void neigh_free_ip4_entries(struct neigh_ip4_entry *entries);
void neigh_free_ip6_entries(struct neigh_ip6_entry *entries);
const char *neigh_strerror(int err);

#endif

