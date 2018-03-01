#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>

#if defined(__FreeBSD__)
#include <net/bpf.h>
#endif /* defined(__FreeBSD__) */

#if defined(__linux__)
#include <linux/if.h>
#include <linux/if_packet.h>
#include <netinet/ether.h>
#endif /* defined(__linux__)*/

#define ETHFRAME_MINSZ 14 /* DST + SRC + type */

#include <lib/net/eth.h>

#if defined(__FreeBSD__)

int eth_sender_init(struct eth_sender *eth, const char *iface) {
  int fd = -1;
  struct ifreq ifr = {{0}};

  if (( fd = open("/dev/bpf", O_WRONLY)) < 0) {
    goto fail;
  }

  snprintf(ifr.ifr_name, IFNAMSIZ, "%s", iface);
  if (ioctl(fd, BIOCSETIF, &ifr)  == -1) {
    goto fail;
  }

  eth->fd = fd;
  eth->lasterr = 0;
  return 0;

fail:
  eth->lasterr = errno;
  if (fd != -1) {
    close(fd);
  }
  return -1;
}

void eth_sender_cleanup(struct eth_sender *eth) {
  if (eth != NULL) {
    close(eth->fd);
    eth->fd = -1;
  }
}

ssize_t eth_sender_write(struct eth_sender *eth, const void *data,
    size_t len) {
  ssize_t ret;

  if (len < ETHFRAME_MINSZ) {
    errno = EINVAL;
    return -1;
  }

  do {
    ret = write(eth->fd, data, len);
  } while(ret == -1 && errno == EINTR);

  if (ret < 0) {
    eth->lasterr = errno;
  }

  return ret;
}

#elif defined(__linux__)

int eth_sender_init(struct eth_sender *eth, const char *iface) {
  int fd = -1;
  struct ifreq ifr;

  if ((fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
    goto fail;
  }

  snprintf(ifr.ifr_name, IFNAMSIZ, "%s", iface);
  if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
    goto fail;
  }

  eth->lasterr = 0;
  eth->fd = fd;
  eth->index = ifr.ifr_ifindex;
  return 0;

fail:
  eth->lasterr = errno;
  if (fd != -1) {
    close(fd);
  }
  return -1;
}

void eth_sender_cleanup(struct eth_sender *eth) {
  if (eth != NULL) {
    close(eth->fd);
    eth->fd = -1;
  }
}

ssize_t eth_sender_write(struct eth_sender *eth, const void *data,
    size_t len) {
  ssize_t ret;
  struct sockaddr_ll lladdr;

  if (len < ETHFRAME_MINSZ) {
    eth->lasterr = EINVAL;
    return -1;
  }

  /* TODO: Move to eth_sender_init?*/
  memset(&lladdr, 0, sizeof(lladdr));
  lladdr.sll_family = AF_PACKET;
  lladdr.sll_ifindex = eth->index;
  memcpy(lladdr.sll_addr, data, ETH_ALEN);

  do {
    ret = sendto(eth->fd, data, len, MSG_NOSIGNAL,
        (struct sockaddr*)&lladdr, sizeof(lladdr));
  } while(ret == -1 && errno == EINTR);

  if (ret < 0) {
    eth->lasterr = errno;
  }

  return ret;
}


#endif /* defined(__FreeBSD__) || defined(__linux__) */

int eth_addr_tostring(const char *eth, char *s, size_t len) {
  return snprintf(s, len, "%02x:%02x:%02x:%02x:%02x:%02x",
      (unsigned int)eth[0],
      (unsigned int)eth[1],
      (unsigned int)eth[2],
      (unsigned int)eth[3],
      (unsigned int)eth[4],
      (unsigned int)eth[5]);
}

int eth_addr_parse(const char *eth, char *dst, size_t dstlen) {
  int ret;

  if (dstlen < 6) {
    return -1;
  }

  ret = sscanf(eth, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", dst, dst + 1, dst + 2,
      dst + 3, dst + 4, dst + 5);
  if (ret != 6) {
    return -1;
  }

  return 0;
}

const char *eth_sender_strerror(struct eth_sender *eth) {
  if (eth->lasterr != 0) {
    return strerror(eth->lasterr);
  } else {
    return "unknown error";
  }
}
