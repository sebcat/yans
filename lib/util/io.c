#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#include <lib/util/io.h>

#define IO_SETERR(io, ...) \
    snprintf((io)->errbuf, sizeof((io)->errbuf), __VA_ARGS__);

#define IO_PERROR(io, func) \
    snprintf((io)->errbuf, sizeof((io)->errbuf), "%s: %s", \
        (func), strerror(errno));

#define CONTROLMSG 0x41424344

const char *io_strerror(io_t *io) {
  return io->errbuf;
}

int io_listen_unix(io_t *io, const char *path) {
  struct sockaddr_un saddr;
  int fd = -1;

  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    IO_PERROR(io, "socket");
    return IO_ERR;
  }
  saddr.sun_family = AF_UNIX;
  snprintf(saddr.sun_path, sizeof(saddr.sun_path), "%s", path);
  unlink(saddr.sun_path);
  if (bind(fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
    IO_PERROR(io, "bind");
    goto fail_close;
  } else if (listen(fd, SOMAXCONN) < 0) {
    IO_PERROR(io, "listen");
    goto fail_close;
  }
  io->fd = fd;
  return IO_OK;
fail_close:
  close(fd);
  return IO_ERR;
}

int io_connect_unix(io_t *io, const char *path) {
  struct sockaddr_un saddr;
  int fd = -1;

  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    IO_PERROR(io, "socket");
    return IO_ERR;
  }
  saddr.sun_family = AF_UNIX;
  snprintf(saddr.sun_path, sizeof(saddr.sun_path), "%s", path);
  if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) != 0) {
    IO_PERROR(io, "connect");
    close(fd);
    return IO_ERR;
  }
  io->fd = fd;
  return IO_OK;
}

int io_accept(io_t *io, io_t *out) {
  int fd;
  do {
    fd = accept(io->fd, NULL, NULL);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0) {
    IO_PERROR(io, "accept");
    return IO_ERR;
  }
  out->fd = fd;
  return IO_OK;
}

int io_writeall(io_t *io, void *data, size_t len) {
  char *cptr = data;
  ssize_t ret;
  while(len > 0) {
    ret = write(io->fd, cptr, len);
    if (ret < 0 && errno == EINTR) {
      continue;
    } else if (ret < 0) {
      IO_PERROR(io, "write");
      return IO_ERR;
    } else if (ret == 0) {
      IO_SETERR(io, "write: unexpected EOF");
      return IO_ERR;
    }
    cptr += ret;
    len -= ret;
  }
  return IO_OK;
}

int io_writevall(io_t *io, struct iovec *iov, int iovcnt) {
  ssize_t ret;
  size_t currvec = 0;
  while (currvec < iovcnt) {
    ret = writev(io->fd, iov+currvec, iovcnt-currvec);
    if (ret < 0 && errno == EINTR) {
      continue;
    } else if (ret < 0) {
      IO_PERROR(io, "writev");
      return IO_ERR;
    } else if (ret == 0) {
      IO_SETERR(io, "writev: unexpected EOF");
      return IO_ERR;
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

int io_readfull(io_t *io, void *data, size_t len) {
  char *cptr = data;
  ssize_t ret;
  while(len > 0) {
    ret = read(io->fd, cptr, len);
    if (ret < 0 && errno == EINTR) {
      continue;
    } else if (ret < 0) {
      IO_PERROR(io, "read");
      return IO_ERR;
    } else if (ret == 0) {
      IO_SETERR(io, "read: unexpected EOF");
      return IO_ERR;
    }
    cptr += ret;
    len -= ret;
  }
  return IO_OK;
}

int io_close(io_t *io) {
  int ret = IO_OK;
  if (io->fd >= 0) {
    if (close(io->fd) < 0) {
      IO_PERROR(io, "close");
      ret = IO_ERR;
    }
    io->fd = -1;
  }
  return ret;
}

int io_tofp(io_t *io, const char *mode, FILE **out) {
  FILE *fp;

  if ((fp = fdopen(io->fd, mode)) == NULL) {
    IO_PERROR(io, "fdopen");
    return IO_ERR;
  }

  io->fd = -1;
  *out = fp;
  return IO_OK;
}

int io_sendfd(io_t *io, int fd) {
  struct iovec iov;
  struct msghdr mhdr = {0};
  struct cmsghdr *cmsg;
  int m = CONTROLMSG;
  ssize_t ret;
  union {
    char buf[CMSG_SPACE(sizeof(int))];
    struct cmsghdr align;
  } u;

  memset(&u, 0, sizeof(u));
  iov.iov_base = &m;
  iov.iov_len = sizeof(m);
  mhdr.msg_iov = &iov;
  mhdr.msg_iovlen = 1;
  mhdr.msg_control = u.buf;
  mhdr.msg_controllen = sizeof(u);
  cmsg = CMSG_FIRSTHDR(&mhdr);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
  memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));
  do {
    ret = sendmsg(io->fd, &mhdr, MSG_NOSIGNAL);
  } while (ret < 0 && errno == EINTR);
  if (ret < 0) {
    IO_PERROR(io, "sendmsg");
    return IO_ERR;
  }
  return IO_OK;
}

int io_recvfd(io_t *io, int *out) {
  struct msghdr mhdr = {0};
  struct iovec iov;
  ssize_t ret;
  struct cmsghdr *cmsg;
  int m = 0;
  int fd = -1;
  union {
    char buf[CMSG_SPACE(sizeof(int))];
    struct cmsghdr align;
  } u;

  iov.iov_base = &m;
  iov.iov_len = sizeof(m);
  mhdr.msg_iov = &iov;
  mhdr.msg_iovlen = 1;
  mhdr.msg_control = u.buf;
  mhdr.msg_controllen = sizeof(u);
  do {
    ret = recvmsg(io->fd, &mhdr, MSG_NOSIGNAL);
  } while (ret < 0 && errno == EINTR);
  if (ret < 0) {
    IO_PERROR(io, "recvmsg");
    return IO_ERR;
  }

  for (cmsg = CMSG_FIRSTHDR(&mhdr); cmsg != NULL;
      cmsg = CMSG_NXTHDR(&mhdr, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET &&
        cmsg->cmsg_type == SCM_RIGHTS &&
        cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
      memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
      break;
    }
  }

  if (fd < 0) {
    IO_SETERR(io, "no file descriptor received");
    return IO_ERR;
  }

  if (m != CONTROLMSG) {
    IO_SETERR(io, "unexpected control message (0x%08x)", m);
    close(fd);
    return IO_ERR;
  }

  *out = fd;
  return IO_OK;
}
