#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <lib/util/zfile.h>
#include <lib/ycl/storecli.h>
#include <apps/scan/opener.h>

/* opener_ctx flags */
#define OPENERCTXF_INITEDYCL (1 << 0)

struct opener_ctx {
  /* internal */
  struct storecli_ctx cli;
  struct ycl_ctx ycl;
  struct ycl_msg msgbuf; /* don't use directly - use opts.msgbuf instead */
  struct opener_opts opts;
  const char *err;
  int flags;
};

static struct opener_ctx opener_;

static inline int opener_error(const char *err) {
  opener_.err = err;
  return -1;
}

int opener_init(struct opener_opts *opts) {
  struct opener_ctx *ctx = &opener_;
  int ret;

  ctx->opts = *opts;
  /* Check if we have a store ID set. If we do we'll use the store */
  if (ctx->opts.store_id != NULL) {
    /* If no socket path was supplied we'll use the default one */
    if (ctx->opts.socket == NULL) {
      ctx->opts.socket = STORECLI_DFLPATH;
    }

    /* If no msgbuf was supplied we'll init our own. msgbufs can be shared
     * between many clients as long as it's not done in parallel */
    if (ctx->opts.msgbuf == NULL) {
      ret = ycl_msg_init(&ctx->msgbuf);
      if (ret != YCL_OK) {
        return opener_error("cl_msg_init failure");
      }
      ctx->opts.msgbuf = &ctx->msgbuf;
    }

    /* Connect to the store */
    ret = ycl_connect(&ctx->ycl, ctx->opts.socket);
    if (ret != YCL_OK) {
      return opener_error(ycl_strerror(&ctx->ycl));
    }

    /* Mark YCL as inited for the cleanup function */
    ctx->flags |= OPENERCTXF_INITEDYCL;

    /* Initialize the store client with the previously inited structs */
    storecli_init(&ctx->cli, &ctx->ycl, ctx->opts.msgbuf);

    /* Enter store - this is problematic because we want to clean up the
     * ycl context which means we cannot use the error from the context
     * as an opener_error unless we copy the string. Set a generic error
     * string for now, or copy the string in the future. */
    ret = storecli_enter(&ctx->cli, ctx->opts.store_id, NULL, 0);
    if (ret < 0) {
      opener_cleanup();
      return opener_error("store enter error");
    }
  }

  return 0;
}

void opener_cleanup() {
  struct opener_ctx *ctx = &opener_;

  if (ctx->opts.msgbuf == &ctx->msgbuf) {
    /* we inited our own msgbuf - clean up*/
    ycl_msg_cleanup(&ctx->msgbuf);
  }

  if (ctx->flags &= OPENERCTXF_INITEDYCL) {
    ycl_close(&ctx->ycl);
    ctx->flags &= ~OPENERCTXF_INITEDYCL;
  }

  memset(ctx, 0, sizeof(*ctx)); /* paranoia */
}

/* Covert fopen(3) style modes to open(2) flags */
static int modestr2flags(const char *modestr, int *outflags) {
  char ch;
  int oflags;
  int mods;

  switch (*modestr++) {
  case 'a':
    oflags = O_WRONLY;
    mods = O_CREAT | O_APPEND;
    break;
  case 'r':
    oflags = O_RDONLY;
    mods = 0;
    break;
  case 'w':
    oflags = O_WRONLY;
    mods = O_CREAT | O_TRUNC;
    break;
  default:
    return -1;
  }

  while ((ch = *modestr++) != '\0') {
    switch(ch) {
    case '+':
      oflags = O_RDWR;
      break;
    case 'b':
      break;
    case 'e':
      mods |= O_CLOEXEC;
      break;
    case 'x':
      mods |= O_EXCL;
      break;
    default:
      return -1;
    }
  }

  if (oflags == O_RDONLY && (mods & O_EXCL)) {
    return -1;
  }

  *outflags = oflags | mods;
  return 0;
}

int opener_fopen(const char *path, const char *mode, int flags,
    FILE **outfp) {
  struct opener_ctx *ctx = &opener_;
  int ret;
  int oflags;
  int fd;
  FILE *fp;

  /* Parse the mode string to open(2) flags*/
  ret = modestr2flags(mode, &oflags);
  if (ret < 0) {
    return opener_error("invalid mode string");
  }

  /* Open the file descriptor */
  if (ctx->opts.store_id == NULL) {
    /* no store - open normally */
    fd = open(path, oflags);
    if (fd < 0) {
      return opener_error(strerror(errno));
    }
  } else {
    /* use store for opening */
    ret = storecli_open(&ctx->cli, path, oflags, &fd);
    if (ret == STORECLI_ERR) {
      return opener_error(storecli_strerror(&ctx->cli));
    }
  }

  /* Open compressed or uncompressed depending on flags */
  if (((flags & OPENERF_COMPRESS_IN)  && (oflags & (O_RDWR|O_RDONLY))) ||
      ((flags & OPENERF_COMPRESS_OUT) && (oflags & (O_RDWR|O_WRONLY)))) {
    fp = zfile_fdopen(fd, mode);
  } else {
    fp = fdopen(fd, mode);
  }

  if (fp == NULL) {
    close(fd);
    return opener_error("fdopen failure"); /* or zfile_fdopen, w/e */
  }

  *outfp = fp;
  return 0;
}

const char *opener_strerr() {
  struct opener_ctx *ctx = &opener_;
  return ctx->err;
}
