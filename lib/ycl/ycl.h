#ifndef YANS_YCL_H__
#define YANS_YCL_H__

#define YCL_ERRBUFSIZ 128
#define YCL_IDATASIZ   32

#define YCL_AGAIN   1
#define YCL_OK      0
#define YCL_ERR    -1

#define YCL_NOFD   -1 /* e.g., ycl_init(ycl, YCL_NOFD) */

/* ycl_msg.flags */
#define YCLMSGF_HASOPTBUF    (1 << 0)

#include <stdint.h>

#include <lib/util/buf.h>

struct ycl_ctx {
  /* -- internal -- */
  int fd;
  int flags;
  size_t max_msgsz;
  char errbuf[YCL_ERRBUFSIZ];
};

struct ycl_msg {
  /* -- internal -- */
  buf_t buf;
  buf_t mbuf; /* secondary buffer used as a scratch-pad in create/parse */
  buf_t optbuf; /* optional buffer used by create for array elements */
  size_t sendoff; /* sendmsg message offset */
  size_t nextoff; /* message offset to next received chunk, if any */
  int flags;      /* flags used internally */
};

/* internal ycl_ctx flags */
#define YCL_EXTERNALFD (1 << 0)

/* accessor macros for individual fields */
#define ycl_fd(ycl) \
    (ycl)->fd
#define ycl_set_externalfd(ycl) \
    (ycl)->flags |= YCL_EXTERNALFD;
#define ycl_msg_bytes(msg) \
    (msg)->buf.data
#define ycl_msg_nbytes(msg) \
    (msg)->buf.len


void ycl_init(struct ycl_ctx *ycl, int fd);
int ycl_connect(struct ycl_ctx *ycl, const char *dst);
int ycl_close(struct ycl_ctx *ycl);
const char *ycl_strerror(struct ycl_ctx *ycl);
int ycl_setnonblock(struct ycl_ctx *ycl, int status);

/* ycl_readmsg is *only* for use with YCL_NOFD ycl_ctx'es on files */
int ycl_readmsg(struct ycl_ctx *ycl, struct ycl_msg *msg, FILE *fp);

int ycl_sendmsg(struct ycl_ctx *ycl, struct ycl_msg *msg);
int ycl_recvmsg(struct ycl_ctx *ycl, struct ycl_msg *msg);
int ycl_recvfd(struct ycl_ctx *ycl, int *fd);
int ycl_sendfd(struct ycl_ctx *ycl, int fd, int err);

int ycl_msg_init(struct ycl_msg *msg);
int ycl_msg_set(struct ycl_msg *msg, const void *data, size_t len);
int ycl_msg_use_optbuf(struct ycl_msg *msg);
void ycl_msg_reset(struct ycl_msg *msg);
void ycl_msg_cleanup(struct ycl_msg *msg);

#endif  /* YANS_YCL_H__ */
