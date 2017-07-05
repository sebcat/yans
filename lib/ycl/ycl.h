#ifndef YANS_YCL_H__
#define YANS_YCL_H__

#define YCL_ERRBUFSIZ 128

struct ycl_ctx {
  int fd;
  char errbuf[YCL_ERRBUFSIZ];
};

int ycl_connect(struct ycl_ctx *ycl, const char *dst);
int ycl_close(struct ycl_ctx *ycl);
int ycl_setnonblock(struct ycl_ctx *ycl, int status);


#endif  /* YANS_YCL_H__ */
