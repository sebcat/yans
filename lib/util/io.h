#ifndef IO_H_
#define IO_H_

#include <sys/types.h>
#include <sys/uio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#include <lib/util/buf.h>

#define IO_TLVMAXSZ       0xffffff
#define IO_TLVTYPE(buf)   (*(uint32_t*)(buf)->data >> 24)
#define IO_TLVLEN(buf)    (*(uint32_t*)(buf)->data & 0xffffff)
#define IO_TLVVAL(buf)    ((buf)->data+4)

#define IO_OK             0
#define IO_ERR           -1

#define IO_ERRBUFSZ 256

typedef struct {
  int fd;
  char errbuf[IO_ERRBUFSZ];
} io_t;

#define io_init(_io, _fd) \
    do { \
      (_io)->fd = (_fd); \
      (_io)->errbuf[0] = '\0'; \
    } while(0);

#define io_fileno(_io) \
    ((_io)->fd)

const char *io_strerror(io_t *io);

int io_open(io_t *io, const char *path, int flags, ...);

int io_listen_unix(io_t *io, const char *path);
int io_connect_unix(io_t *io, const char *path);
int io_accept(io_t *io, io_t *out);

int io_writeall(io_t *io, void *data, size_t len);
int io_writevall(io_t *io, struct iovec *iov, int iovcnt);
int io_readfull(io_t *io, void *data, size_t len);

int io_close(io_t *io);

/* converts an io_t to a FILE*. After this, the io_t instance disowns the
 * underlying file descriptor, as ownership of it is passed to the FILE*. */
int io_tofp(io_t *io, const char *mode, FILE **out);

int io_sendfd(io_t *io, int fd);
int io_recvfd(io_t *io, int *out);

#endif
