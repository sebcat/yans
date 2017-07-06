#ifndef YANS_YCL_H__
#define YANS_YCL_H__

#define YCL_ERRBUFSIZ 128
#define YCL_IDATASIZ   32

#define YCL_AGAIN   1
#define YCL_OK      0
#define YCL_ERR    -1

struct ycl_ctx {
  int fd;
  char errbuf[YCL_ERRBUFSIZ];
};

struct ycl_msg {
  char idata[YCL_IDATASIZ]; /* internal */
};

int ycl_connect(struct ycl_ctx *ycl, const char *dst);
int ycl_close(struct ycl_ctx *ycl);
int ycl_setnonblock(struct ycl_ctx *ycl, int status);
int ycl_sendmsg(struct ycl_ctx *ycl, struct ycl_msg *msg);
int ycl_recvmsg(struct ycl_ctx *ycl, struct ycl_msg *msg);


#endif  /* YANS_YCL_H__ */
