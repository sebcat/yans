#ifndef IO_H_
#define IO_H_

#include <sys/types.h>
#include <sys/uio.h>
#include <stdint.h>
#include <unistd.h>

#include <lib/util/buf.h>

#define IO_TLVMAXSZ       0xffffff
#define IO_TLVTYPE(buf)   (*(uint32_t*)(buf)->data >> 24)
#define IO_TLVLEN(buf)    (*(uint32_t*)(buf)->data & 0xffffff)
#define IO_TLVVAL(buf)    ((buf)->data+4)

#define IO_OK             0
#define IO_ERR           -1   /* general read/write failure */
#define IO_UNEXPECTEDEOF -2
#define IO_MSGTOOBIG     -3
#define IO_MEM           -4

const char *io_strerror(int err);

int io_writeall(int fd, void *data, size_t len);
int io_writevall(int fd, struct iovec *iov, int iovcnt);
int io_readfull(int fd, void *data, size_t len);

/* io_*tlv - TLV implementation
 * types can be 0-0xff inclusive
 * length can be 0-0xffffff inclusive, excluding the TLV header */
int io_readtlv(int fd, buf_t *buf);
int io_writetlv(int fd, int type, struct iovec *iov, int iovcnt);

#endif
