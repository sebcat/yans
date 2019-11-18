/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <curl/curl.h>

#include <lib/net/tcpsrc.h>
#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>
#include <apps/webfetch/fetch.h>

#define DEFAULT_CONNECT_TIMEOUT 10 /* in seconds */
#define DEFAULT_MAXSIZE (1024*1024)

static size_t writefunc(char *ptr, size_t size, size_t nmemb, void *data) {
  struct fetch_transfer *t;
  size_t totsz;
  int ret;
  char *crlf;

  t = data;
  totsz = size * nmemb;
  if (t->recvbuf.len + totsz >= t->maxsize) {
    totsz = t->maxsize - t->recvbuf.len;
  }

  ret = buf_adata(&t->recvbuf, ptr, totsz);
  if (ret < 0) {
    totsz = 0;
  }

  if (t->bodyoff == 0) {
    crlf = memmem(t->recvbuf.data + t->scanoff, t->recvbuf.len - t->scanoff,
        "\r\n\r\n", 4);
    if (crlf != NULL) {
      t->bodyoff = (crlf + 4) - t->recvbuf.data;
      /* TODO: return 0 if the header has a content-type we don't like */
    } else if (t->recvbuf.len > 4) {
      t->scanoff = t->recvbuf.len - 4;
    }
  }

  return totsz;
}

static curl_socket_t opensockfunc(void *data, curlsocktype purpose,
    struct curl_sockaddr *address) {
  struct fetch_transfer *t;
  int ret;

  t = data;
  ret = tcpsrc_connect(t->tcpsrc, &address->addr);
  return ret >= 0 ? ret : CURL_SOCKET_BAD;
}

static int sockoptfunc(void *data, curl_socket_t curlfd,
    curlsocktype purpose) {
  return CURL_SOCKOPT_ALREADY_CONNECTED;
}

static CURL *create_default_easy(struct fetch_transfer *t) {
  CURL *e;
  long off = 0;

  e = curl_easy_init();
  if (e == NULL) {
    return NULL;
  }

  /* accept HTTP, HTTPS urls and include the HTTP header in the response */
  curl_easy_setopt(e, CURLOPT_PROTOCOLS, CURLPROTO_HTTP|CURLPROTO_HTTPS);
  curl_easy_setopt(e, CURLOPT_HEADER, 1L);

  /* CURLOPT_ACCEPT_ENCODING: "" == all supported encodings. This gives
   * us Accept-Encoding: deflate, gzip or similar, and handles decompr
   * for us automatically */
  curl_easy_setopt(e, CURLOPT_ACCEPT_ENCODING, "");

  /* set transfer speed limit */
  curl_easy_setopt(e, CURLOPT_LOW_SPEED_LIMIT, 100L);
  curl_easy_setopt(e, CURLOPT_LOW_SPEED_TIME, 10L);
  curl_easy_setopt(e, CURLOPT_CONNECTTIMEOUT, DEFAULT_CONNECT_TIMEOUT);

  /* register the callback for received data */
  curl_easy_setopt(e, CURLOPT_WRITEFUNCTION, writefunc);
  curl_easy_setopt(e, CURLOPT_WRITEDATA, t);

  /* ensure connections are made using tcpsrc(4) */
  curl_easy_setopt(e, CURLOPT_OPENSOCKETFUNCTION, opensockfunc);
  curl_easy_setopt(e, CURLOPT_OPENSOCKETDATA, t);
  curl_easy_setopt(e, CURLOPT_SOCKOPTFUNCTION, sockoptfunc);

  /* Disable TLS certificate validation */
  /* TODO: Make optional, but keep default no-verify */
  curl_easy_setopt(e, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(e, CURLOPT_SSL_VERIFYPEER, 0L);

  /* disable signal masking code, assumes SIGPIPE is handled elsewhere */
  /* NB: this probably also disables SIGALRM */
  curl_easy_setopt(e, CURLOPT_NOSIGNAL, 1L);

  /* tcpsrc(4) should already have set this for us. Turning it off here
   * saves us a syscall/connection */
  curl_easy_setopt(e, CURLOPT_TCP_NODELAY, off);

  /* TODO: set shared cookie store */
  return e;
}

int fetch_init(struct fetch_ctx *ctx, struct fetch_opts *opts) {
  int i;
  int ret;
  struct fetch_transfer *t;

  if (opts->nfetchers <= 0) {
    return -1;
  }

  memset(ctx, 0, sizeof(*ctx));
  ctx->opts = *opts;
  if (ctx->opts.maxsize == 0) {
    ctx->opts.maxsize = DEFAULT_MAXSIZE;
  }

  ctx->fetchers = calloc(ctx->opts.nfetchers,
    sizeof(struct fetch_transfer));
  if (ctx->fetchers == NULL) {
    goto fail;
  }

  ctx->multi = curl_multi_init();
  if (ctx->multi == NULL) {
    goto fail;
  }

  for (i = 0; i < opts->nfetchers; i++) {
    t = &ctx->fetchers[i];
    t->tcpsrc = opts->tcpsrc;
    t->maxsize = ctx->opts.maxsize;
    t->easy = create_default_easy(t);
    if (t->easy == NULL) {
      goto fail;
    }

    if (!buf_init(&t->urlbuf, 2048)) {
      goto fail;
    }

    if (!buf_init(&t->connecttobuf, 2048)) {
      goto fail;
    }

    if (!buf_init(&t->recvbuf, DEFAULT_MAXSIZE)) {
      goto fail;
    }
  }

  ycl_init(&ctx->ycl, YCL_NOFD);
  ret = ycl_msg_init(&ctx->msgbuf);
  if (ret != YCL_OK) {
    goto fail;
  }


  return 0;
fail:
  fetch_cleanup(ctx);
  return -1;
}

void fetch_cleanup(struct fetch_ctx *ctx) {
  int i;
  struct fetch_transfer *t;

  for (i = 0; i < ctx->opts.nfetchers; i++) {
    t = &ctx->fetchers[i];
    if (t->easy != NULL) {
      curl_easy_cleanup(t->easy);
      t->easy = NULL;
      buf_cleanup(&t->urlbuf);
      buf_cleanup(&t->connecttobuf);
      buf_cleanup(&t->recvbuf);
    }
  }

  if (ctx->multi != NULL) {
    curl_multi_cleanup(ctx->multi);
    ctx->multi = NULL;
  }

  if (ctx->fetchers != NULL) {
    free(ctx->fetchers);
    ctx->fetchers = NULL;
  }

  ycl_close(&ctx->ycl);
  ycl_msg_cleanup(&ctx->msgbuf);
}

static int ahost(buf_t *buf, const void *data, size_t len) {
  int ret;

  if (len > 0 && strchr(data, ':') != NULL) {
    ret = buf_achar(buf, '[');
    ret |= buf_adata(buf, data, len);
    ret |= buf_achar(buf, ']');
  } else {
    ret = buf_adata(buf, data, len);
  }

  return ret ? -1 : 0;
}

static int build_connect_to(buf_t *buf, struct ycl_msg_httpmsg *msg) {
  int ret;

  buf_clear(buf);
  ret = ahost(buf, msg->hostname.data, msg->hostname.len);
  ret |= buf_adata(buf, "::", 2);
  ret |= ahost(buf, msg->addr.data, msg->addr.len);
  ret |= buf_achar(buf, ':');
  ret |= buf_achar(buf, '\0');

  return ret ? -1 : 0;
}

static int build_request_url(buf_t *urlbuf, struct ycl_msg_httpmsg *msg) {
  int ret;

  buf_clear(urlbuf);

  /* do we already have a URL?  If so, copy it and we're done! */
  if (msg->url.len > 0) {
    ret = buf_adata(urlbuf, msg->url.data, msg->url.len);
    if (ret != 0) {
      return -1;
    }
    buf_achar(urlbuf, '\0');
    return 0;
  }

  if (msg->scheme.len == 0 || msg->hostname.len == 0) {
    return -1;
  }

  buf_adata(urlbuf, msg->scheme.data, msg->scheme.len);
  buf_adata(urlbuf, "://", 3);
  ahost(urlbuf, msg->hostname.data, msg->hostname.len);

  /* append the port if it is set and it's not the default one */
  if (msg->port.len > 0) {
    if ((strcmp(msg->scheme.data, "http") == 0 &&
         strcmp(msg->port.data, "80") != 0) ||
        (strcmp(msg->scheme.data, "https") == 0 &&
         strcmp(msg->port.data, "443") != 0)) {
      buf_achar(urlbuf, ':');
      buf_adata(urlbuf, msg->port.data, msg->port.len);
    }
  }

  /* append path */
  if (msg->path.len > 0) {
    if (msg->path.data[0] != '/') {
      buf_achar(urlbuf, '/');
    }
    buf_adata(urlbuf, msg->path.data, msg->path.len);
  } else {
    buf_achar(urlbuf, '/');
  }

  /* append params */
  if (msg->params.len > 0) {
    if (msg->params.data[0] != '?') {
      buf_achar(urlbuf, '?');
    }
    buf_adata(urlbuf, msg->params.data, msg->params.len);
  }

  buf_achar(urlbuf, '\0');
  return 0;
}

static struct fetch_transfer *find_transfer(struct fetch_ctx *ctx,
    CURL *e) {
  struct fetch_transfer *t = NULL;
  int i;

  for (i = 0; i < ctx->opts.nfetchers; i++) {
    if (ctx->fetchers[i].easy == e) {
      t = &ctx->fetchers[i];
      break;
    }
  }

  return t;
}

static int complete_transfer(struct fetch_ctx *ctx,
    struct fetch_transfer *t) {

  if (ctx->opts.on_completed) {
    ctx->opts.on_completed(t, ctx->opts.completeddata);
  }

  buf_clear(&t->urlbuf);
  buf_clear(&t->connecttobuf);
  buf_clear(&t->recvbuf);
  curl_slist_free_all(t->ctslist);
  t->dstaddr[0] = '\0';
  t->hostname[0] = '\0';
  t->ctslist = NULL;
  t->scanoff = 0;
  t->bodyoff = 0;
  return 0;
}

static int start_transfer(struct fetch_ctx *ctx,
    struct fetch_transfer *t) {
  int ret;
  struct ycl_msg_httpmsg httpmsg;

  ret = ycl_readmsg(&ctx->ycl, &ctx->msgbuf, ctx->opts.infp);
  if (ret != YCL_OK) {
    return -1;
  }

  ret = ycl_msg_parse_httpmsg(&ctx->msgbuf, &httpmsg);
  if (ret != YCL_OK) {
    return -1;
  }

  t->service_id = httpmsg.service_id;
  ret = build_request_url(&t->urlbuf, &httpmsg);
  if (ret < 0) {
    return -1;
  }

  strncpy(t->dstaddr, httpmsg.addr.data, sizeof(t->dstaddr));
  t->dstaddr[sizeof(t->dstaddr)-1] = '\0';
  strncpy(t->hostname, httpmsg.hostname.data, sizeof(t->hostname));
  t->hostname[sizeof(t->hostname)-1] = '\0';

  /* build the string which tells curl how to map the url host to a
   * specific address */
  ret = build_connect_to(&t->connecttobuf, &httpmsg);
  if (ret < 0) {
    return -1;
  }

  t->ctslist = curl_slist_append(t->ctslist, t->connecttobuf.data);
  if (t->ctslist == NULL) {
    return -1;
  }

  curl_easy_setopt(t->easy, CURLOPT_CONNECT_TO, t->ctslist);
  curl_easy_setopt(t->easy, CURLOPT_URL, t->urlbuf.data);
  curl_multi_add_handle(ctx->multi, t->easy);

  return 0;
}

static int fetch_select(struct fetch_ctx *ctx) {
  fd_set fdread;
  fd_set fdwrite;
  fd_set fdexcept;
  int ret, maxfd;
  struct timeval to;
  struct timeval wait = {0, 100*1000};
  long timeo = -1;

  FD_ZERO(&fdread);
  FD_ZERO(&fdwrite);
  FD_ZERO(&fdexcept);

  curl_multi_timeout(ctx->multi, &timeo);
  if (timeo < 0) {
    to.tv_sec =1;
    to.tv_usec = 0;
  } else {
    to.tv_sec = timeo / 1000;
    to.tv_usec = (timeo % 1000) * 1000;
  }

  ret = curl_multi_fdset(ctx->multi, &fdread, &fdwrite, &fdexcept, &maxfd);
  if (ret != CURLM_OK) {
    return -1;
  }

again:
  if (maxfd == -1) {
    ret = select(maxfd+1, NULL, NULL, NULL, &wait);
  } else {
    ret = select(maxfd+1, &fdread, &fdwrite, &fdexcept, &to);
  }

  if (ret < 0 && errno == EINTR) {
    goto again;
  }

  return ret;
}

int fetch_run(struct fetch_ctx *ctx) {
  int i;
  int nrunning = 0;
  int ret;
  struct fetch_transfer *t;
  CURLMsg *msg;
  int msgs_left = 0; /* TODO: Remove? */
  int source_done = 0;

  /* start up all fetchers */
  for (i = 0; i < ctx->opts.nfetchers; i++) {
    ret = start_transfer(ctx, &ctx->fetchers[i]);
    if (ret < 0) {
      break; /* NB: We don't distinguish between completion and failure */
    }
  }

  curl_multi_perform(ctx->multi, &nrunning);
  do {
    ret = fetch_select(ctx);
    if (ret < 0) {
      goto fail;
    }

    curl_multi_perform(ctx->multi, &nrunning);
    while ((msg = curl_multi_info_read(ctx->multi, &msgs_left)) != NULL) {
      if (msg->msg == CURLMSG_DONE) {
        t = find_transfer(ctx, msg->easy_handle);
        assert(t != NULL); /* Not finding the transfer would be bad */
        complete_transfer(ctx, t);
        curl_multi_remove_handle(ctx->multi, msg->easy_handle);
        if (!source_done) {
          ret = start_transfer(ctx, t);
          if (ret < 0) {
            source_done = 1;
          }
        }
      }
      curl_multi_perform(ctx->multi, &nrunning);
    }
  } while (nrunning > 0);

  return 0;
fail:
  return -1;
}
