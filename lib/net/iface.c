#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <lib/net/iface.h>

#if defined(__linux__)
#define LINKAF AF_PACKET
#else
#define LINKAF AF_LINK
#endif

#if defined(__FreeBSD__)

#include <net/if_dl.h>

void copy_iface_addr(struct iface_entry *out, struct sockaddr *sa) {
  const struct sockaddr_dl *dl;

  assert(sa->sa_family == LINKAF);
  dl = (struct sockaddr_dl*)sa;
  memcpy(out->addr, LLADDR(dl), sizeof(out->addr));
}
#else

/* TODO: headers */

void copy_iface_addr(struct iface_entry *out, struct sockaddr *sa) {
  const struct sockaddr_ll *ll;

  assert(sa->sa_family == LINKAF);
  ll = (struct sockaddr_ll*)sa;
  memcpy(out->addr, ll->sll_addr, sizeof(out->addr));

}
#endif

int iface_init(struct iface *iface) {
  int ret;

  ret = getifaddrs(&iface->addrs);
  if (ret < 0) {
    iface->err = errno;
    return -1;
  }

  iface->curr = iface->addrs;
  iface->err = 0;
  return 0;
}

int iface_next(struct iface *iface, struct iface_entry *out) {
  if (iface->curr == NULL) {
    return 0;
  }

  /* go to the next link-level entry, if any */
  while (iface->curr->ifa_addr->sa_family != LINKAF) {
    iface->curr = iface->curr->ifa_next;
    if (iface->curr == NULL) {
      return 0;
    }
  }

  strncpy(out->name, iface->curr->ifa_name, sizeof(out->name));
  out->name[IFACE_NAMESZ-1] = '\0';
  copy_iface_addr(out, iface->curr->ifa_addr);
  out->index = (int)if_nametoindex(out->name);
  if (out->index <= 0) {
    out->index = -1;
  }

  iface->curr = iface->curr->ifa_next;
  return 1;
}

void iface_cleanup(struct iface *iface) {
  if (iface && iface->addrs) {
    freeifaddrs(iface->addrs);
    iface->addrs = NULL;
    iface->curr = NULL;
    iface->err = 0;
  }
}

const char *iface_strerror(struct iface *iface) {
  return strerror(iface->err);
}
