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
#include <errno.h>

#include <lib/util/os.h>
#include <lib/ycl/ycl_msg.h>
#include <lib/ycl/yclcli_store.h>

int yclcli_store_enter(struct yclcli_ctx *ctx, const char *id,
    const char **out_id) {
  struct ycl_msg_store_req reqmsg = {{0}};
  struct ycl_msg_status_resp respmsg = {{0}};
  int ret;

  reqmsg.action.data = "enter";
  reqmsg.action.len = sizeof("enter") - 1;
  reqmsg.store_id.data = id;
  reqmsg.store_id.len = id ? strlen(id) : 0;
  ret = ycl_msg_create_store_req(ctx->msgbuf, &reqmsg);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, "ycl_msg_create_store_enter failure");
  }

  ret = ycl_sendmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ycl_msg_reset(ctx->msgbuf);
  ret = ycl_recvmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ret = ycl_msg_parse_status_resp(ctx->msgbuf, &respmsg);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, "failed to parse enter response");
  }

  if (respmsg.errmsg.data != NULL && *respmsg.errmsg.data != '\0') {
    return yclcli_seterr(ctx, respmsg.errmsg.data);
  }

  if (out_id) {
    *out_id = respmsg.okmsg.data;
  }

  return YCL_OK;
}

int yclcli_store_open(struct yclcli_ctx *ctx, const char *path, int flags,
    int *outfd) {
  struct ycl_msg_store_entered_req openmsg = {{0}};
  int ret;

  openmsg.action.data = "open";
  openmsg.action.len = 5;
  openmsg.open_path.data = path;
  openmsg.open_path.len = strlen(path);
  openmsg.open_flags = flags;
  ret = ycl_msg_create_store_entered_req(ctx->msgbuf, &openmsg);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, "failed to serialize open request\n");
  }

  ret = ycl_sendmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ret = ycl_recvfd(&ctx->ycl, outfd);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  return YCL_OK;
}

int yclcli_store_fopen(struct yclcli_ctx *ctx, const char *path,
    const char *mode, FILE **outfp) {
  int ret;
  int oflags;
  int fd;
  FILE *fp;

  ret = os_mode2flags(mode, &oflags);
  if (ret < 0) {
    return yclcli_seterr(ctx, "invalid mode string");
  }

  ret = yclcli_store_open(ctx, path, oflags, &fd);
  if (ret != YCL_OK) {
    return ret;
  }

  fp = fdopen(fd, mode);
  if (!fp) {
    close(fd);
    return yclcli_seterr(ctx, strerror(errno));
  }

  if (outfp) {
    *outfp = fp;
  } else {
    fclose(fp);
  }

  return YCL_OK;
}

int yclcli_store_index(struct yclcli_ctx *ctx, size_t before, size_t nelems,
    int *outfd) {
  struct ycl_msg_store_req reqmsg = {{0}};
  int ret;

  reqmsg.action.data = "index";
  reqmsg.action.len = sizeof("index") - 1;
  ret = ycl_msg_create_store_req(ctx->msgbuf, &reqmsg);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, "ycl_msg_create_store_enter failure");
  }

  ret = ycl_sendmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ycl_msg_reset(ctx->msgbuf);
  ret = ycl_recvfd(&ctx->ycl, outfd);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  return YCL_OK;
}

int yclcli_store_list(struct yclcli_ctx *ctx, const char *id,
    const char *must_match, const char **result, size_t *resultlen) {
  struct ycl_msg_store_req reqmsg = {{0}};
  struct ycl_msg_store_list respmsg = {{0}};
  int ret;

  reqmsg.action.data = "list";
  reqmsg.action.len = sizeof("list") - 1;
  reqmsg.store_id.data = id;
  reqmsg.store_id.len = id ? strlen(id) : 0;
  reqmsg.list_must_match.data = must_match;
  reqmsg.list_must_match.len = must_match ? strlen(must_match) : 0;
  ret = ycl_msg_create_store_req(ctx->msgbuf, &reqmsg);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, "ycl_msg_create_store_req failure");
  }

  ret = ycl_sendmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ycl_msg_reset(ctx->msgbuf);
  ret = ycl_recvmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ret = ycl_msg_parse_store_list(ctx->msgbuf, &respmsg);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, "failed to parse store list response");
  }

  if (respmsg.errmsg.len > 0) {
    return yclcli_seterr(ctx, respmsg.errmsg.data);
  }

  *result = respmsg.entries.data;
  *resultlen = respmsg.entries.len;
  return YCL_OK;
}

int yclcli_store_rename(struct yclcli_ctx *ctx, const char *from,
    const char *to) {
  struct ycl_msg_store_entered_req renamemsg = {{0}};
  struct ycl_msg_status_resp respmsg = {{0}};
  int ret;

  renamemsg.action.data = "rename";
  renamemsg.action.len = 7;
  renamemsg.rename_from.data = from;
  renamemsg.rename_from.len = from ? strlen(from) : 0;
  renamemsg.rename_to.data = to;
  renamemsg.rename_to.len = to ? strlen(to) : 0;
  ret = ycl_msg_create_store_entered_req(ctx->msgbuf, &renamemsg);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, "failed to serialize rename request");
  }

  ret = ycl_sendmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ycl_msg_reset(ctx->msgbuf);
  ret = ycl_recvmsg(&ctx->ycl, ctx->msgbuf);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, ycl_strerror(&ctx->ycl));
  }

  ret = ycl_msg_parse_status_resp(ctx->msgbuf, &respmsg);
  if (ret != YCL_OK) {
    return yclcli_seterr(ctx, "failed to parse rename response\n");
  }

  if (respmsg.errmsg.data != NULL && *respmsg.errmsg.data != '\0') {
    return yclcli_seterr(ctx, respmsg.errmsg.data);
  }

  return YCL_OK;
}
