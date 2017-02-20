#include <errno.h>
#include <string.h>

#include <lib/util/io.h>

const char *io_strerror(int err) {
  switch(err) {
    case IO_OK:
      return "success";
    case IO_ERR:
      return "read/write failure";
    case IO_UNEXPECTEDEOF:
      return "unexpected eof";
    case IO_MSGTOOBIG:
      return "message too big";
    case IO_MEM:
      return "memory allocation error";
    default:
      return "unknown error";
  }
}

int io_writeall(int fd, void *data, size_t len) {
  char *cptr = data;
  ssize_t ret;
  while(len > 0) {
    ret = write(fd, cptr, len);
    if (ret < 0 && errno == EINTR) {
      continue;
    } else if (ret < 0) {
      return IO_ERR;
    } else if (ret == 0) {
      return IO_UNEXPECTEDEOF;
    }
    cptr += ret;
    len -= ret;
  }
  return IO_OK;
}

int io_writevall(int fd, struct iovec *iov, int iovcnt) {
  ssize_t ret;
  size_t currvec = 0;

  while (currvec < iovcnt) {
    ret = writev(fd, iov+currvec, iovcnt-currvec);
    if (ret < 0 && errno == EINTR) {
      continue;
    } else if (ret < 0) {
      return IO_ERR;
    } else if (ret == 0) {
      return IO_UNEXPECTEDEOF;
    }

    /* advance currvec */
    while (ret >= iov[currvec].iov_len) {
      ret -= iov[currvec++].iov_len;
    }

    /* advance iov_base of last iov element if needed */
    if (currvec < iovcnt && ret > 0) {
      iov[currvec].iov_base = (char*)iov[currvec].iov_base + ret;
      iov[currvec].iov_len -= ret;
    }
  }
  return IO_OK;
}

int io_readfull(int fd, void *data, size_t len) {
  char *cptr = data;
  ssize_t ret;
  while(len > 0) {
    ret = read(fd, cptr, len);
    if (ret < 0 && errno == EINTR) {
      continue;
    } else if (ret < 0) {
      return IO_ERR;
    } else if (ret == 0) {
      return IO_UNEXPECTEDEOF;
    }
    cptr += ret;
    len -= ret;
  }
  return IO_OK;
}

int io_readtlv(int fd, buf_t *buf) {
  int ret;
  uint32_t len;
  buf_clear(buf);
  if (buf_reserve(buf, sizeof(uint32_t)) < 0) {
    return IO_MEM;
  }

  if ((ret = io_readfull(fd, buf->data, sizeof(uint32_t))) != IO_OK) {
    return ret;
  }

  len = *(uint32_t*)buf->data & 0xffffff;
  if (buf_reserve(buf, sizeof(uint32_t) + (size_t)len) < 0) {
    return IO_MEM;
  }

  if ((ret = io_readfull(fd, buf->data+sizeof(uint32_t), (size_t)len)) < 0) {
    return ret;
  }

  buf->len = sizeof(uint32_t) + (size_t)len;
  return IO_OK;
}

#define IOVBUFSZ 16

int io_writetlv(int fd, int type, struct iovec *iov, int iovcnt) {
  int i, niovecs, ret;
  size_t len = 0;
  uint32_t header;
  struct iovec iovbuf[IOVBUFSZ];

  if (iovcnt == 0) {
    return 0;
  }

  for(i=0; i<iovcnt; i++) {
    len += iov[i].iov_len;
  }

  if (len > IO_TLVMAXSZ) {
    return IO_MSGTOOBIG;
  }

  /* write the first IOVBUFSZ-1 iovecs, with the header iovec prepended */
  niovecs = iovcnt < IOVBUFSZ ? iovcnt : IOVBUFSZ-1;
  memcpy(iovbuf+1, iov, niovecs);
  header = (((type&0xff)<<24) | len);
  iovbuf[0].iov_base = &header;
  iovbuf[0].iov_len = sizeof(header);
  if ((ret = io_writevall(fd, iovbuf, niovecs+1)) != IO_OK) {
    return ret;
  }

  /* write the rest of the iovecs, if any */
  iovcnt -= niovecs;
  if (iovcnt > 0) {
    iov += niovecs;
    return io_writevall(fd, iov, iovcnt);
  } else {
    return IO_OK;
  }
}
