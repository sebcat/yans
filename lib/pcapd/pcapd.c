#include <lib/pcapd/pcapd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <stdint.h>
#include <stdio.h>

/* TLV types */
#define PCAPD_TOPEN 0
#define PCAPD_TOK   1
#define PCAPD_TERR  2

#define CLEARERRBUF(pcapd) ((pcapd)->errbuf[0] = '\0')

const char *pcapd_strerror(pcapd_t *pcapd) {
  return pcapd->errbuf;
}

int pcapd_listen(pcapd_t *pcapd, const char *path) {
  struct sockaddr_un saddr;

  CLEARERRBUF(pcapd);
  pcapd->fd = -1;
  if ((pcapd->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf), "socket: %s",
        strerror(errno));
    goto fail;
  }
  saddr.sun_family = AF_UNIX;
  snprintf(saddr.sun_path, sizeof(saddr.sun_path), "%s", path);
  unlink(saddr.sun_path);
  if (bind(pcapd->fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf), "bind: %s",
        strerror(errno));
    goto fail;
  } else if (listen(pcapd->fd, SOMAXCONN) < 0) {
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf), "listen: %s",
        strerror(errno));
    goto fail;
  }
  return PCAPD_OK;

fail:
  if (pcapd->fd >= 0) {
    close(pcapd->fd);
    pcapd->fd = -1;
  }
  return PCAPD_ERR;
}

int pcapd_connect(pcapd_t *pcapd, const char *path) {
  struct sockaddr_un saddr;

  CLEARERRBUF(pcapd);
  pcapd->fd = -1;
  if ((pcapd->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf), "socket: %s",
        strerror(errno));
    goto fail;
  }

  saddr.sun_family = AF_UNIX;
  snprintf(saddr.sun_path, sizeof(saddr.sun_path), "%s", path);
  if (connect(pcapd->fd, (struct sockaddr *)&saddr, sizeof(saddr)) != 0) {
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf), "connect: %s",
        strerror(errno));
    goto fail;
  }

  return PCAPD_OK;

fail:
  if (pcapd->fd >= 0) {
    close(pcapd->fd);
    pcapd->fd = -1;
  }
  return PCAPD_ERR;
}

int pcapd_accept(pcapd_t *pcapd, pcapd_t *cli) {
  int fd;

  CLEARERRBUF(pcapd);
  do {
    fd = accept(pcapd->fd, NULL, NULL);
  } while(fd < 0 && errno == EINTR);

  if (fd < 0) {
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf), "accept: %s",
        strerror(errno));
    return PCAPD_ERR;
  }

  CLEARERRBUF(cli);
  cli->fd = fd;
  return PCAPD_OK;
}

int pcapd_wropen(pcapd_t *pcapd, const char *iface, const char *filter) {
  int ret;
  uint32_t ifacelen, filterlen;
  struct iovec iov[4];

  CLEARERRBUF(pcapd);
  ifacelen = (uint32_t)strlen(iface);
  filterlen = (uint32_t)strlen(filter);
  iov[0].iov_base = &ifacelen;
  iov[0].iov_len = sizeof(ifacelen);
  iov[1].iov_base = &filterlen;
  iov[1].iov_len = sizeof(filterlen);
  iov[2].iov_base = (char*)iface;
  iov[2].iov_len = (size_t)ifacelen;
  iov[3].iov_base = (char*)filter;
  iov[3].iov_len = (size_t)filterlen;
  if ((ret = io_writetlv(pcapd->fd, PCAPD_TOPEN, iov,
      sizeof(iov)/sizeof(struct iovec))) != IO_OK) {
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf), "io_writetlv: %s",
        io_strerror(ret));
    goto fail;
  }

  /* TODO: implement */

fail:
  return PCAPD_ERR;
}

int pcapd_close(pcapd_t *pcapd) {
  int ret = PCAPD_OK;
  CLEARERRBUF(pcapd);
  if (pcapd->fd >= 0) {
    if (close(pcapd->fd) < 0) {
      snprintf(pcapd->errbuf, sizeof(pcapd->errbuf), "close: %s",
          strerror(errno));
      ret = PCAPD_ERR;
    }
    pcapd->fd = -1;
  }
  return ret;
}
