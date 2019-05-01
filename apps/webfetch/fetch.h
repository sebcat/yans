#ifndef WEBFETCH_FETCH_H__
#define WEBFETCH_FETCH_H__

#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>

#define fetch_transfer_dstaddr(t) \
    ((t)->dstaddr)
#define fetch_transfer_hostname(t) \
    ((t)->hostname)
#define fetch_transfer_url(t) \
    ((t)->urlbuf.len ? (t)->urlbuf.data : "")
#define fetch_transfer_urllen(t) \
    ((t)->urlbuf.len ? (t)->urlbuf.len-1 : 0)
#define fetch_transfer_header(t) \
    ((t)->recvbuf.len > 0 ? (t)->recvbuf.data : "")
#define fetch_transfer_headerlen(t) \
    ((t)->bodyoff > 4 ? ((t)->bodyoff - 4) : (t)->recvbuf.len)
#define fetch_transfer_body(t) \
    ((t)->bodyoff > 0 ? (t)->recvbuf.data + (t)->bodyoff : "")
#define fetch_transfer_bodylen(t) \
    ((t)->bodyoff > 0 ? (t)->recvbuf.len - (t)->bodyoff : 0)

struct fetch_transfer {
  char hostname[256]; /* hostname in textual representation */
  char dstaddr[64]; /* destination address in textual representation */
  buf_t urlbuf;
  buf_t connecttobuf;
  void *easy;
  struct curl_slist *ctslist;
  struct tcpsrc_ctx *tcpsrc;

  buf_t recvbuf;
  size_t maxsize;

  size_t scanoff; /* where to start looking for end-of-header in recvbuf */
  size_t bodyoff; /* if >0: offset to the response body in recvbuf */
};

struct fetch_opts {
  FILE *infp;
  struct tcpsrc_ctx *tcpsrc;
  int nfetchers;
  size_t maxsize;
  void (*on_completed)(struct fetch_transfer *, void *);
  void *completeddata;
};

struct fetch_ctx {
  struct fetch_opts opts;
  struct ycl_ctx ycl;
  struct ycl_msg msgbuf;
  void *multi;
  struct fetch_transfer *fetchers;
};

int fetch_init(struct fetch_ctx *ctx, struct fetch_opts *opts);
void fetch_cleanup(struct fetch_ctx *ctx);
int fetch_run(struct fetch_ctx *ctx);

#endif
