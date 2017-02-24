#include <lib/pcapd/pcapd.h>
#include <lib/util/io.h>
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


int pcapd_init(pcapd_t *pcapd, size_t bufsz) {
  pcapd->fd = -1;
  pcapd->errbuf[0] = '\0';
  if (buf_init(&pcapd->buf, bufsz) == NULL) {
    return PCAPD_ERR;
  }
  return PCAPD_OK;
}


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

int pcapd_rdopen(pcapd_t *pcapd) {
  int ret, status = PCAPD_ERR;
  uint32_t type, len, ifacelen, filterlen;
  char *val;

  CLEARERRBUF(pcapd);
  buf_clear(&pcapd->buf);
  if ((ret = io_readtlv(pcapd->fd, &pcapd->buf)) != IO_OK) {
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf), "pcapd_rdopen: %s",
        io_strerror(ret));
    goto exit;
  }
  type = IO_TLVTYPE(&pcapd->buf);
  if (type != PCAPD_TOPEN) {
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf),
        "pcapd_rdopen: unknown type (%d)", type);
    goto exit;
  }
  len = IO_TLVLEN(&pcapd->buf);
  if (len < sizeof(uint32_t)+2) {
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf),
        "pcapd_rdopen: message too short (was:%u)", len);
    goto exit;
  }
  val = IO_TLVVAL(&pcapd->buf);
  ifacelen = *(uint32_t*)val;
  filterlen = *(uint32_t*)(val+4);
  if (ifacelen == 0 || filterlen == 0 ||
      ifacelen + filterlen + sizeof(uint32_t)*2 != len) {
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf),
        "pcapd_rdopen: missmatched length fields");
    goto exit;
  }

  if (*(val + sizeof(uint32_t)*2 + ifacelen-1) != '\0') {
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf),
        "pcapd_rdopen: ifacelen missing trailing nullbyte");
    goto exit;
  } else if (*(val + sizeof(uint32_t)*2 + ifacelen + filterlen-1) != '\0') {
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf),
        "pcapd_rdopen: filterlen missing trailing nullbyte");
    goto exit;
  }

  status = PCAPD_OK;
exit:
  return status;
}

int pcapd_wropen(pcapd_t *pcapd, const char *iface, const char *filter) {
  int ret, status = PCAPD_ERR;
  uint32_t ifacelen, filterlen, type;
  struct iovec iov[4];

  if (filter == NULL) {
    filter = "";
  }

  CLEARERRBUF(pcapd);
  /* send PCAPD_TOPEN to pcapd listener
   * header    ifacelen   filterlen    iface      filter
   * uint32_t  uint32_t   uint32_t     ifacelen   filterlen
   *  - includes the trailing \0 bytes in iface, filter
   *  - header is managed by io_writetlv */
  ifacelen = (uint32_t)strlen(iface) + 1;
  filterlen = (uint32_t)strlen(filter) + 1;
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
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf), "req pcapd_wropen: %s",
        io_strerror(ret));
    goto exit;
  }

  /* read the response */
  buf_clear(&pcapd->buf);
  if ((ret = io_readtlv(pcapd->fd, &pcapd->buf)) != IO_OK) {
    snprintf(pcapd->errbuf, sizeof(pcapd->errbuf), "resp pcapd_wropen: %s",
        io_strerror(ret));
    goto exit;
  }

  /* validate response */
  type = IO_TLVTYPE(&pcapd->buf);
  if (type != PCAPD_TOK) {
    if (type == PCAPD_TERR) {
      snprintf(pcapd->errbuf, sizeof(pcapd->errbuf),
          "resp pcapd_wropen: %*s", IO_TLVLEN(&pcapd->buf),
          IO_TLVVAL(&pcapd->buf));
    } else {
      snprintf(pcapd->errbuf, sizeof(pcapd->errbuf),
          "resp pcapd_wropen: unknown type (%u)", type);
    }
    goto exit;
  }

  status = PCAPD_OK;
exit:
  return status;
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
