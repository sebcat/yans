#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <lib/net/iface.h>

#if defined(__linux__)
#define LINKAF AF_PACKET
#else
#define LINKAF AF_LINK
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

const char *iface_next(struct iface *iface) {
  char *ret = NULL;

  if (iface->curr == NULL) {
    goto done;
  }

  /* go to the next link-level entry, if any */
  while (iface->curr->ifa_addr->sa_family != LINKAF) {
    iface->curr = iface->curr->ifa_next;
    if (iface->curr == NULL) {
      goto done;
    }
  }

  ret = iface->curr->ifa_name;
  iface->curr = iface->curr->ifa_next;
done:
  return ret;
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
