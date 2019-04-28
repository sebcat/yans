#ifndef WEBFETCH_FETCH_H__
#define WEBFETCH_FETCH_H__

#include <curl/curl.h>

struct fetch_transfer {

};

struct fetch_opts {
  FILE *infp;
  struct tcpsrc_ctx *tcpsrc;
  int nfetchers;
};

struct fetch_ctx {
  struct fetch_opts opts;
};

int fetch_init(struct fetch_ctx *ctx, struct fetch_opts *opts);
void fetch_cleanup(struct fetch_ctx *ctx);
int fetch_run(struct fetch_ctx *ctx);

#endif
