#ifndef YANS_NEIGH_H__
#define YANS_NEIGH_H__

#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <lib/net/ip.h>

#define NEIGH_IFNAMSIZ 16 /* including trailing \0 */
#define NEIGH_ETHSIZ 6

struct neigh_entry {
  ip_addr_t ipaddr;
  char iface[NEIGH_IFNAMSIZ];
  char hwaddr[NEIGH_ETHSIZ];
};

struct neigh_entry *neigh_get_entries(size_t *nentries, int *err);
void neigh_free_entries(struct neigh_entry *entries);
const char *neigh_strerror(int err);

#endif

