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
  struct ifaddrs *addrs = NULL;
  struct ifaddrs *curr = NULL;
  size_t nentries = 0;
  struct iface_entry *entries = NULL;
  struct iface_entry *out;
  int pos;

  /* init the result to a known state */
  ifs->err = 0;
  ifs->nentries = 0;
  ifs->entries = NULL;

  /* get a list of all the links */
  ret = getifaddrs(&addrs);
  if (ret < 0) {
    ifs->err = errno;
    return -1;
  }

  /* count the number of entries, goto done if there's none */
  for (curr = addrs; curr != NULL; curr = curr->ifa_next) {
    if (curr->ifa_addr->sa_family == LINKAF) {
      nentries++;
    }
  }
  if (nentries <= 0) {
    goto done;
  }

  /* allocate the entry table */
  entries = malloc(sizeof(struct iface_entry) * nentries);
  if (entries == NULL) {
    ifs->err = errno;
    goto done;
  }

  /* copy the entries to the table */
  for (pos = 0, curr = addrs; curr != NULL; curr = curr->ifa_next) {
    if (curr->ifa_addr->sa_family != LINKAF) {
      continue;
    }

    out = &entries[pos++];
    strncpy(out->name, curr->ifa_name, sizeof(out->name));
    out->name[IFACE_NAMESZ-1] = '\0';
    copy_iface_addr(out, curr->ifa_addr);
    out->index = (int)if_nametoindex(out->name);
    out->flags = curr->ifa_flags;
    if (out->index <= 0) {
      out->index = -1;
    }
  }

  /* success */
  ifs->nentries = nentries;
  ifs->entries = entries;
  entries = NULL;
done:
  if (addrs != NULL) {
    freeifaddrs(addrs);
  }

  if (entries != NULL) {
    free(entries);
  }

  return ifs->err == 0 ? 0 : -1;
}

void iface_cleanup(struct iface_entries *ifs) {
  if (ifs && ifs->entries) {
    free(ifs->entries);
    ifs->entries = NULL;
  }
  ifs->nentries = 0;
  ifs->err = 0;
}

const char *iface_strerror(struct iface_entries *ifs) {
  return strerror(ifs->err);
}
