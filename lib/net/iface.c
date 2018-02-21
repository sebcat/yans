#include <sys/types.h>
#include <sys/socket.h>
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

static void copy_iface_addr(struct iface_entry *out, struct sockaddr *sa) {
  const struct sockaddr_dl *dl;

  assert(sa->sa_family == LINKAF);
  dl = (struct sockaddr_dl*)sa;
  memcpy(out->addr, LLADDR(dl), sizeof(out->addr));
}
#else

#include <linux/if_packet.h>

static void copy_iface_addr(struct iface_entry *out, struct sockaddr *sa) {
  const struct sockaddr_ll *ll;

  assert(sa->sa_family == LINKAF);
  ll = (struct sockaddr_ll*)sa;
  memcpy(out->addr, ll->sll_addr, sizeof(out->addr));
}
#endif

int iface_init(struct iface_entries *ifs) {
  int ret;
  int fam;
  int iface_pos = 0;
  int ip_pos = 0;
  struct ifaddrs *addrs = NULL;
  struct ifaddrs *curr = NULL;
  size_t nifaces = 0;
  struct iface_entry *ifaces = NULL;
  struct iface_srcaddr *ipsrcs = NULL;
  size_t nipsrcs = 0;

  /* init the result to a known state */
  ifs->err = 0;
  ifs->ifaces = NULL;
  ifs->nifaces = 0;
  ifs->ipsrcs = NULL;
  ifs->nipsrcs = 0;

  /* get a list of all the links */
  ret = getifaddrs(&addrs);
  if (ret < 0) {
    ifs->err = errno;
    return -1;
  }

  /* count the number of entries, goto done if there's none */
  for (curr = addrs; curr != NULL; curr = curr->ifa_next) {
    fam = curr->ifa_addr->sa_family;
    if (fam == LINKAF) {
      nifaces++;
    } else if (fam == AF_INET || fam == AF_INET6) {
      nipsrcs++;
    }
  }
  if (nifaces <= 0) {
    goto done;
  }

  /* allocate the tables */
  ifaces = calloc(nifaces, sizeof(struct iface_entry));
  if (ifaces == NULL) {
    ifs->err = errno;
    goto done;
  }

  if (nipsrcs > 0) {
    ipsrcs = calloc(nipsrcs, sizeof(struct iface_srcaddr));
    if (ipsrcs == NULL) {
      ifs->err = errno;
      goto done;
    }
  }

  /* copy the entries to the table */
  for (curr = addrs; curr != NULL; curr = curr->ifa_next) {
    fam = curr->ifa_addr->sa_family;
    if (fam == LINKAF) {
      struct iface_entry *out = &ifaces[iface_pos++];
      strncpy(out->name, curr->ifa_name, sizeof(out->name));
      out->name[IFACE_NAMESZ-1] = '\0';
      copy_iface_addr(out, curr->ifa_addr);
      out->index = (int)if_nametoindex(out->name);
      out->flags = curr->ifa_flags;
      if (out->index <= 0) {
        out->index = -1;
      }
    } else if (fam == AF_INET) {
      struct iface_srcaddr *out = &ipsrcs[ip_pos++];
      strncpy(out->ifname, curr->ifa_name, sizeof(out->ifname));
      out->ifname[IFACE_NAMESZ-1] = '\0';
      memcpy(&out->addr.u.sin, curr->ifa_addr, sizeof(struct sockaddr_in));
      memcpy(&out->mask.u.sin, curr->ifa_netmask, sizeof(struct sockaddr_in));
    } else if (fam == AF_INET6) {
      struct iface_srcaddr *out = &ipsrcs[ip_pos++];
      strncpy(out->ifname, curr->ifa_name, sizeof(out->ifname));
      out->ifname[IFACE_NAMESZ-1] = '\0';
      memcpy(&out->addr.u.sin6, curr->ifa_addr, sizeof(struct sockaddr_in6));
      memcpy(&out->mask.u.sin6, curr->ifa_netmask,
          sizeof(struct sockaddr_in6));
    }
  }

  /* success */
  ifs->nifaces = nifaces;
  ifs->ifaces = ifaces;
  ifs->ipsrcs = ipsrcs;
  ifs->nipsrcs = nipsrcs;
  ifaces = NULL;
  ipsrcs = NULL;
done:
  if (addrs != NULL) {
    freeifaddrs(addrs);
  }

  if (ifaces != NULL) {
    free(ifaces);
  }

  if (ipsrcs != NULL) {
    free(ipsrcs);
  }

  return ifs->err == 0 ? 0 : -1;
}

void iface_cleanup(struct iface_entries *ifs) {
  if (ifs) {
    if (ifs->ifaces) {
      free(ifs->ifaces);
      ifs->ifaces = NULL;
    }
    if (ifs->ipsrcs) {
      free(ifs->ipsrcs);
      ifs->ipsrcs = NULL;
    }
    ifs->nifaces = 0;
    ifs->nipsrcs = 0;
    ifs->err = 0;
  }
}

const char *iface_strerror(struct iface_entries *ifs) {
  return strerror(ifs->err);
}
