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
#ifndef REAPLAN_H__
#define REAPLAN_H__

#include <openssl/ssl.h>
#include <lib/util/buf.h>
#include <lib/util/idset.h>

/* reaplan status codes */
#define REAPLAN_ERR -1
#define REAPLAN_OK   0

/* signals from on_connect callback */
#define REAPLANC_ERR  -1
#define REAPLANC_OK    0
#define REAPLANC_DONE  1
#define REAPLANC_WAIT  2

/* reaplan connection flags */
#define REAPLAN_READABLE         (1 << 0)
#define REAPLAN_WRITABLE_ONESHOT (1 << 1)
#define REAPLAN_TLS_HANDSHAKE    (1 << 2)

struct reaplan_ctx;

struct reaplan_conn {
  /* internal */
  time_t expires;
  void *udata;
  int fd;
  unsigned int flags;      /* set flags */
  unsigned int rflags;     /* requested flags */
  unsigned int nwritten;   /* number of bytes written (XXX: wrap-around) */
  unsigned int nread;      /* number of bytes read (XXX: wrap-around) */
  SSL *ssl;                /* TLS connection data, if any */
};

struct reaplan_opts {
  /* reaplan callbacks */
  int (*on_connect)(struct reaplan_ctx *, struct reaplan_conn *);
  ssize_t (*on_readable)(struct reaplan_ctx *, struct reaplan_conn *);
  ssize_t (*on_writable)(struct reaplan_ctx *, struct reaplan_conn *);
  void (*on_done)(struct reaplan_ctx *, struct reaplan_conn *);

  void *udata;                /* user-data per reaplan instance */
  int timeout;                /* read/write/connect timeout, in seconds */
  unsigned int max_clients;   /* maximum # of concurrent connections */
  unsigned int connects_per_tick; /* # of initiated connections per tick */
  unsigned int mdelay_per_tick;   /* # of milliseconds to delay per tick */
  SSL_CTX *ssl_ctx;               /* optional, if set - use TLS */
};

struct reaplan_ctx {
  /* internal */
  struct kevent *evs;
  struct idset_ctx *ids;      /* idset to keep track of used conn slots */
  struct reaplan_conn *conns; /* array of per-connection state structs */
  struct reaplan_conn **closeconns; /* conns queued for close(2)  */
  struct reaplan_opts opts;   /* caller supplied options */
  time_t last_time;           /* last fetched time, in seconds */
  int fd;                     /* kqueue fd */
  int active_conns;           /* # of currently active connections */
  int ncloseconns;            /* # of currently used closeconns slots */
  int timer_active;           /* 1 if the timeout timer is active */
  int connect_done;           /* 1 if on_connect returned REAPLANC_DONE */
  int throttle_connect;       /* 1 if no new connections are to be made */
};

int reaplan_init(struct reaplan_ctx *ctx,
    const struct reaplan_opts *opts);
void reaplan_cleanup(struct reaplan_ctx *ctx);
int reaplan_run(struct reaplan_ctx *ctx);


int reaplan_register_conn(struct reaplan_ctx *ctx,
    struct reaplan_conn *conn, int fd, unsigned int flags,
    const char *name);
int reaplan_conn_read(struct reaplan_conn *conn, void *data, int len);
int reaplan_conn_write(struct reaplan_conn *conn, void *data, int len);

void reaplan_conn_append_cert_chain(struct reaplan_conn *conn, buf_t *out);

static inline void *reaplan_get_udata(const struct reaplan_ctx *ctx) {
  return ctx->opts.udata;
}

static inline int reaplan_conn_get_fd(const struct reaplan_conn *conn) {
  return conn->fd;
}

static inline void reaplan_conn_set_udata(struct reaplan_conn *conn,
    void *udata) {
  conn->udata = udata;
}

static inline void *reaplan_conn_get_udata(struct reaplan_conn *conn) {
  return conn->udata;
}

static inline unsigned int reaplan_conn_get_nwritten(
    struct reaplan_conn *conn) {
  return conn->nwritten;
}

static inline unsigned int reaplan_conn_get_nread(
    struct reaplan_conn *conn) {
  return conn->nread;
}

#endif
