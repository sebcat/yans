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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include <lib/util/os.h>
#include <lib/util/zfile.h>
#include <lib/ycl/opener.h>

/* opener_ctx flags */
#define OPENERCTXF_INITEDYCL (1 << 0)

static inline int opener_error(struct opener_ctx *ctx, const char *err) {
  ctx->err = err;
  return -1;
}

int opener_init(struct opener_ctx *ctx, struct opener_opts *opts) {
  int ret;
  const char *id;

  memset(ctx, 0, sizeof(*ctx));
  ctx->opts = *opts;

  /* if store_id is set, it needs to be a non-empty string */
  if (ctx->opts.store_id != NULL && *ctx->opts.store_id == '\0') {
    ctx->opts.store_id = NULL;
  }

  /* If we have a store_id, or if the used has requested to create a
   * store, we set up a store to use. */
  if (ctx->opts.store_id != NULL || (opts->flags & OPENER_FCREAT) != 0) {
    /* If no socket path was supplied we'll use the default one */
    if (ctx->opts.socket == NULL) {
      ctx->opts.socket = STORECLI_DFLPATH;
    }

    /* If no msgbuf was supplied we'll init our own. msgbufs can be shared
     * between many clients as long as it's not done in parallel */
    if (ctx->opts.msgbuf == NULL) {
      ret = ycl_msg_init(&ctx->msgbuf);
      if (ret != YCL_OK) {
        return opener_error(ctx, "cl_msg_init failure");
      }
      ctx->opts.msgbuf = &ctx->msgbuf;
    }

    /* Initialize the store client and connect to stored */
    yclcli_init(&ctx->cli, ctx->opts.msgbuf);
    ret = yclcli_connect(&ctx->cli, ctx->opts.socket);
    if (ret != YCL_OK) {
      return opener_error(ctx, ycl_strerror(&ctx->cli.ycl));
    }

    /* Mark YCL as inited for the cleanup function */
    ctx->flags |= OPENERCTXF_INITEDYCL;

    /* Enter store - this is problematic because we want to clean up the
     * ycl context which means we cannot use the error from the context
     * as an opener_error unless we copy the string. Set a generic error
     * string for now, or copy the string in the future. */
    ret = yclcli_store_enter(&ctx->cli, ctx->opts.store_id, &id);
    if (ret < 0) {
      opener_cleanup(ctx);
      return opener_error(ctx, "unable to enter store");
    }

    /* If we didn't have a store ID set (e.g., if OPENER_FCREAT was set),
     * copy the ID */
    if (ctx->opts.store_id == NULL) {
      ctx->allotted_store_id = strdup(id);
      if (ctx->allotted_store_id == NULL) {
        opener_cleanup(ctx);
        return opener_error(ctx, "unable to store allotted store ID");
      }

      ctx->opts.store_id = ctx->allotted_store_id;
    }
  }

  return 0;
}

void opener_cleanup(struct opener_ctx *ctx) {
  if (ctx->opts.msgbuf == &ctx->msgbuf) {
    /* we inited our own msgbuf - clean up*/
    ycl_msg_cleanup(&ctx->msgbuf);
  }

  if (ctx->flags & OPENERCTXF_INITEDYCL) {
    yclcli_close(&ctx->cli);
  }

  if (ctx->allotted_store_id != NULL) {
    free(ctx->allotted_store_id);
  }

  memset(ctx, 0, sizeof(*ctx));
}

int opener_open(struct opener_ctx *ctx, const char *path, int flags,
    int *use_zlib, int *outfd) {
  int fd;
  int ret;

  assert(ctx != NULL);
  assert(path != NULL);

  /* Files starting with zlib: are special - they are opened for compressed
   * reading/writing. The zlib:prefix is not a part of the actual name. */
  if (strncmp(path, "zlib:", 5) == 0) {
    path += 5;
    if (use_zlib) {
      *use_zlib = 1;
    }
  }

  /* Open/set the file descriptor */
  if (path[0] == '-' && path[1] == '\0') {
    /* use standard in/out */
    if ((flags & O_ACCMODE) == O_RDONLY) {
      fd = STDIN_FILENO;
    } else {
      fd = STDOUT_FILENO;
    }
  } else if (ctx->opts.store_id == NULL) {
    /* no store - open normally. Default to rwxr-xr-x */
    fd = open(path, flags, 0755);
    if (fd < 0) {
      return opener_error(ctx, strerror(errno));
    }
  } else {
    /* use store for opening */
    ret = yclcli_store_open(&ctx->cli, path, flags, &fd);
    if (ret == YCL_ERR) {
      return opener_error(ctx, yclcli_strerror(&ctx->cli));
    }
  }

  *outfd = fd;
  return 0;
}

int opener_fopen(struct opener_ctx *ctx, const char *path,
    const char *mode, FILE **outfp) {
  int ret;
  int oflags;
  int fd;
  FILE *fp;
  int use_zlib = 0;

  assert(path != NULL);
  assert(mode != NULL);
  assert(outfp != NULL);

  /* Parse the mode string to open(2) flags*/
  ret = os_mode2flags(mode, &oflags);
  if (ret < 0) {
    return opener_error(ctx, "invalid mode string");
  }

  /* Open a file descriptor. opener_open sets the error, if any. */
  ret = opener_open(ctx, path, oflags, &use_zlib, &fd);
  if (ret != 0) {
    return ret;
  }

  /* Use compression or not, depending on whether the zlib: prefix
   * is set */
  if (use_zlib) {
    fp = zfile_fdopen(fd, mode);
  } else {
    fp = fdopen(fd, mode);
  }

  if (fp == NULL) {
    close(fd);
    return opener_error(ctx, "fdopen failure"); /* or zfile_fdopen, w/e */
  }

  *outfp = fp;
  return 0;
}

const char *opener_strerror(struct opener_ctx *ctx) {
  return ctx->err;
}
