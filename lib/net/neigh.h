#ifndef YANS_NEIGH_H__
#define YANS_NEIGH_H__

#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define NEIGH_IFNAMSIZ 16 /* including trailing \0 */
#define NEIGH_ETHSIZ 6

struct neigh_entry {
  union {
    struct sockaddr sa;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } u;
  char iface[NEIGH_IFNAMSIZ];
  char hwaddr[NEIGH_ETHSIZ];
};

struct neigh_entry *neigh_get_entries(size_t *nentries, int *err);
void neigh_free_entries(struct neigh_entry *entries);
const char *neigh_strerror(int err);

#endif

