#ifndef YANS_YCL_H__
#define YANS_YCL_H__

#define YCL_ERRBUFSIZ 128
#define YCL_IDATASIZ   32

#define YCL_AGAIN   1
#define YCL_OK      0
#define YCL_ERR    -1

struct ycl_ctx {
  /* -- internal -- */
  int fd;
  char errbuf[YCL_ERRBUFSIZ];
};

struct ycl_msg {
  /* -- internal -- */
  char idata[YCL_IDATASIZ];
}; /* TODO: Align? */

/* accessor macros for struct ycl_ctx, use these instead of accessing the
 * fields directly */
#define ycl_fd(ycl) \
    (ycl)->fd
#define ycl_strerror(ycl) \
    (ycl)->errbuf

int ycl_connect(struct ycl_ctx *ycl, const char *dst);
int ycl_close(struct ycl_ctx *ycl);
int ycl_setnonblock(struct ycl_ctx *ycl, int status);

int ycl_sendmsg(struct ycl_ctx *ycl, struct ycl_msg *msg);
int ycl_recvmsg(struct ycl_ctx *ycl, struct ycl_msg *msg);
int ycl_sendfd(struct ycl_ctx *ycl, int fd);

int ycl_msg_init(struct ycl_msg *msg);
void ycl_msg_reset(struct ycl_msg *msg);
void ycl_msg_cleanup(struct ycl_msg *msg);

int ycl_msg_create_pcap_req(struct ycl_msg *msg, const char *iface,
    const char *filter);
int ycl_msg_create_pcap_close(struct ycl_msg *msg);

int ycl_msg_create_ethframe_req(struct ycl_msg *msg, const char *iface,
    size_t nframes, const char **frames, size_t *frameslen);

int ycl_msg_parse_status_resp(struct ycl_msg *msg, const char **okmsg,
    const char **errmsg);



#endif  /* YANS_YCL_H__ */
