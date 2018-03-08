#include <stdlib.h>
#include <stdio.h>

#include "yclgen.h"

static const char *typ2c(enum yclgen_field_type t) {
  switch (t) {
  case FT_DATAARR:
    return "struct ycl_data *";
  case FT_STRARR:
    return "const char **";
  case FT_LONGARR:
    return "long *";
  case FT_DATA:
    return "struct ycl_data ";
  case FT_STR:
    return "const char *";
  case FT_LONG:
    return "long ";
  default:
    return "?err_type?";
  }
}

static int write_struct(FILE *fp, struct yclgen_msg *msg) {
  struct yclgen_field *field;
  int ret = 0;
  int i;

  ret = fprintf(fp, "struct ycl_msg_%s {\n", msg->name);
  if (ret < 0) {
    return -1;
  }

  for (i = 0; i < msg->nfields; i++) {
    field = &msg->fields[i];
    ret = fprintf(fp, "  %s%s;\n", typ2c(field->typ), field->name);
    if (ret < 0) {
      return -1;
    }
    if (field->typ == FT_DATAARR || field->typ == FT_STRARR ||
        field->typ == FT_LONGARR) {
      ret = fprintf(fp, "  size_t n%s;\n", field->name);
      if (ret <= 0) {
        return -1;
      }
    }
  }

  ret = fprintf(fp, "};\n\n");
  if (ret < 0) {
    return -1;
  }

  return 0;
}

static int write_fdecl(FILE *fp, struct yclgen_msg *msg) {
  int ret;

  ret = fprintf(fp,
      "int ycl_msg_parse_%s(struct ycl_msg *msg,\n"
      "    struct ycl_msg_%s *r);\n"
      "int ycl_msg_create_%s(struct ycl_msg *msg,\n"
      "    struct ycl_msg_%s *r);\n\n",
      msg->name, msg->name, msg->name, msg->name);

  return ret;
}

static int write_header(FILE *fp, struct yclgen_msg *msgs) {
  int ret;
  struct yclgen_msg *curr;
  static const char pre[] =
      "/* auto-generated by yclgen - DO NOT EDIT */\n"
      "#ifndef YCLGEN_MSGS_H__\n"
      "#define YCLGEN_MSGS_H__\n"
      "#include <stddef.h>\n"
      "#include <lib/ycl/ycl.h>\n"
      "\n"
      "struct ycl_data {\n"
      "  size_t len;\n"
      "  const char *data;\n"
      "};\n\n";
  static const char post[] =
      "#endif /* YCLGEN_MSGS_H__ */\n";

  if (fwrite(pre, sizeof(pre)-1, 1, fp) != 1) {
    return -1;
  }

  /* data types */
  for (curr = msgs; curr != NULL; curr = curr->next) {
    ret = write_struct(fp, curr);
    if (ret < 0) {
      return -1;
    }
  }

  /* function declarations */
  for (curr = msgs; curr != NULL; curr = curr->next) {
    ret = write_fdecl(fp, curr);
    if (ret < 0) {
      return -1;
    }
  }

  if (fwrite(post, sizeof(post)-1, 1, fp) != 1) {
    return -1;
  }

  return 0;
}

static int write_static_impl(struct yclgen_ctx *ctx, FILE *fp) {
  int ret;

  /* s2l, l2s */
  if ((ctx->flags & MSGF_HASLONG) || (ctx->flags & MSGF_HASLARR)) {
    ret = fprintf(fp,
        "static long s2l(const char *s) {\n"
        "  long res = 0;\n"
        "  char ch;\n"
        "\n"
        "  while ((ch = *s) >= '0' && ch <= '7') {\n"
        "    res = res << 3;\n"
        "    res += ch - '0';\n"
        "    s++;\n"
        "  }\n"
        "\n"
        "  return res;\n"
        "}\n"
        "\n"
        "static char *l2s(char *s, size_t len, long l) {\n"
        "  while (len > 0) {\n"
        "    len--;\n"
        "    s[len] = '0' + (l & 0x07);\n"
        "    l = l >> 3;\n"
        "    if (l == 0) {\n"
        "      break;\n"
        "    }\n"
        "  }\n"
        "\n"
        "  return s + len;\n"
        "}\n\n");
    if (ret <= 0) {
      return -1;
    }
  }

  /* load_datas */
  if (ctx->flags & MSGF_HASDARR) {
    ret = fprintf(fp,
        "static int load_datas(buf_t *b, char *curr, size_t len,\n"
        "    size_t *outoffset, size_t *nelems) {\n"
        "  int ret;\n"
        "  char *elem;\n"
        "  size_t elemlen;\n"
        "  size_t offset;\n"
        "  size_t count;\n"
        "  struct ycl_data tmp;\n"
        "\n"
        "  ret = buf_align(b);\n"
        "  if (ret < 0) {\n"
        "    return YCL_ERR;\n"
        "  }\n"
        "\n"
        "  offset = b->len;\n"
        "  count = 0;\n"
        "  while (len > 0) {\n"
        "    ret = netstring_next(&elem, &elemlen, &curr, &len);\n"
        "    if (ret != NETSTRING_OK) {\n"
        "      return YCL_ERR;\n"
        "    }\n"
        "    tmp.data = elem;\n"
        "    tmp.len = elemlen;\n"
        "    ret = buf_adata(b, &tmp, sizeof(tmp));\n"
        "    if (ret < 0) {\n"
        "      return YCL_ERR;\n"
        "    }\n"
        "    count++;\n"
        "  }\n"
        "\n"
        "  *outoffset = offset;\n"
        "  *nelems = count;\n"
        "  return YCL_OK;\n"
        "}\n"
        "\n");
    if (ret <= 0) {
      return -1;
    }
  }

  /* load_strs */
  if (ctx->flags & MSGF_HASSARR) {
    ret = fprintf(fp,
        "static int load_strs(buf_t *b, char *curr, size_t len,\n"
        "    size_t *outoffset, size_t *nelems) {\n"
        "  int ret;\n"
        "  char *elem;\n"
        "  size_t elemlen;\n"
        "  size_t offset;\n"
        "  size_t count;\n"
        "\n"
        "  ret = buf_align(b);\n"
        "  if (ret < 0) {\n"
        "    return YCL_ERR;\n"
        "  }\n"
        "\n"
        "  offset = b->len;\n"
        "  count = 0;\n"
        "  while (len > 0) {\n"
        "    ret = netstring_next(&elem, &elemlen, &curr, &len);\n"
        "    if (ret != NETSTRING_OK) {\n"
        "      return YCL_ERR;\n"
        "    }\n"
        "    ret = buf_adata(b, &elem, sizeof(elem));\n"
        "    if (ret < 0) {\n"
        "      return YCL_ERR;\n"
        "    }\n"
        "    count++;\n"
        "  }\n"
        "\n"
        "  *outoffset = offset;\n"
        "  *nelems = count;\n"
        "  return YCL_OK;\n"
        "}\n"
        "\n");
    if (ret <= 0) {
      return -1;
    }
  }

  /* load_longs */
  if (ctx->flags & MSGF_HASLARR) {
    ret = fprintf(fp,
        "static int load_longs(buf_t *b, char *curr, size_t len,\n"
        "    size_t *outoffset, size_t *nelems) {\n"
        "  int ret;\n"
        "  char *elem;\n"
        "  size_t elemlen;\n"
        "  size_t offset;\n"
        "  size_t count;\n"
        "  long val;\n"
        "\n"
        "  ret = buf_align(b);\n"
        "  if (ret < 0) {\n"
        "    return YCL_ERR;\n"
        "  }\n"
        "\n"
        "  offset = b->len;\n"
        "  count = 0;\n"
        "  while (len > 0) {\n"
        "    ret = netstring_next(&elem, &elemlen, &curr, &len);\n"
        "    if (ret != NETSTRING_OK) {\n"
        "      return YCL_ERR;\n"
        "    }\n"
        "    val = s2l(elem);\n"
        "    ret = buf_adata(b, &val, sizeof(val));\n"
        "    if (ret < 0) {\n"
        "      return YCL_ERR;\n"
        "    }\n"
        "    count++;\n"
        "  }\n"
        "\n"
        "  *outoffset = offset;\n"
        "  *nelems = count;\n"
        "  return YCL_OK;\n"
        "}\n"
        "\n");
    if (ret <= 0) {
      return -1;
    }
  }

  return 0;
}

static int write_empty_create_impl(FILE *fp, struct yclgen_msg *msg) {
  int ret;

  ret = fprintf(fp,
      "int ycl_msg_create_%s(struct ycl_msg *msg,\n"
      "    struct ycl_msg_%s *r) {\n"
      "  int ret;\n"
      "  ycl_msg_reset(msg);\n"
      "  ret = buf_adata(&msg->buf, \"0:,\", 3);\n"
      "  if (ret < 0) {\n"
      "    return YCL_ERR;\n"
      "  }\n"
      "  return YCL_OK;\n"
      "}\n\n", msg->name, msg->name);
  if (ret <= 0) {
    return -1;
  }

  return 0;
}

static int write_create_impl(FILE *fp, struct yclgen_msg *msg) {
  struct yclgen_field *f;
  int ret;
  int i;

  if (msg->nfields == 0) {
    return write_empty_create_impl(fp, msg);
  }

  ret = fprintf(fp,
      "int ycl_msg_create_%s(struct ycl_msg *msg,\n"
      "    struct ycl_msg_%s *r) {\n"
      "  int ret;\n"
      "  int status = YCL_ERR;\n"
      "\n"
      "%s"
      "  ycl_msg_reset(msg);\n"
      , msg->name, msg->name,
      (msg->flags & (MSGF_HASDARR | MSGF_HASSARR | MSGF_HASLARR)) ?
          "  if (ycl_msg_use_optbuf(msg) < 0) {\n"
          "    goto done;\n"
          "  }\n"
          "\n" : "");
  if (ret <= 0) {
    return -1;
  }

  for (i = 0; i < msg->nfields; i++) {
    f = &msg->fields[i];
    switch (f->typ) {
    case FT_STR:
      ret = fprintf(fp,
        "  if (r->%s != NULL) {\n"
        "    ret = netstring_append_pair(&msg->mbuf, \"%s\",\n"
        "        sizeof(\"%s\")-1, r->%s, strlen(r->%s));\n"
        "    if (ret != NETSTRING_OK) {\n"
        "      goto done;\n"
        "    }\n"
        "  }\n\n",
        f->name, f->name, f->name, f->name, f->name);
      if (ret <= 0) {
        return -1;
      }
      break;
    case FT_DATA:
      ret = fprintf(fp,
        "  if (r->%s.len > 0) {\n"
        "    ret = netstring_append_pair(&msg->mbuf, \"%s\",\n"
        "        sizeof(\"%s\")-1, r->%s.data, r->%s.len);\n"
        "    if (ret != NETSTRING_OK) {\n"
        "      goto done;\n"
        "    }\n"
        "  }\n\n",
        f->name, f->name, f->name, f->name, f->name);
      if (ret <= 0) {
        return -1;
      }
      break;
    case FT_LONG:
      ret = fprintf(fp,
        "  if (r->%s != 0) {\n"
        "    char numbuf[48];\n"
        "    char *num;\n"
        "\n"
        "    num = l2s(numbuf, sizeof(numbuf), r->%s);\n"
        "    ret = netstring_append_pair(&msg->mbuf, \"%s\",\n"
        "        sizeof(\"%s\")-1, num, numbuf + sizeof(numbuf) - num);\n"
        "    if (ret != NETSTRING_OK) {\n"
        "      goto done;\n"
        "    }\n"
        "  }\n\n",
        f->name, f->name, f->name, f->name);
      if (ret <= 0) {
        return -1;
      }
      break;
    case FT_STRARR:
      ret = fprintf(fp,
        "  if (r->%s != NULL && r->n%s > 0) {\n"
        "    const char **ptr;\n"
        "    size_t i;\n"
        "    size_t nelems;\n"
        "\n"
        "    ptr = r->%s;\n"
        "    nelems = r->n%s;\n"
        "    buf_clear(&msg->optbuf);\n"
        "    for (i = 0; i < nelems; i++) {\n"
        "      ret = netstring_append_buf(&msg->optbuf, ptr[i],\n"
        "          strlen(ptr[i]));\n"
        "      if (ret != NETSTRING_OK) {\n"
        "        goto done;\n"
        "      }\n"
        "    }\n"
        "    ret = netstring_append_pair(&msg->mbuf, \"%s\",\n"
        "        sizeof(\"%s\")-1, msg->optbuf.data, msg->optbuf.len);\n"
        "    if (ret != NETSTRING_OK) {\n"
        "      goto done;\n"
        "    }\n"
        "  }\n\n",
        f->name, f->name, f->name, f->name, f->name, f->name);
      if (ret <= 0) {
        return -1;
      }
      break;
    case FT_DATAARR:
      ret = fprintf(fp,
        "  if (r->%s != NULL && r->n%s > 0) {\n"
        "    struct ycl_data *ptr;\n"
        "    size_t i;\n"
        "    size_t nelems;\n"
        "\n"
        "    ptr = r->%s;\n"
        "    nelems = r->n%s;\n"
        "    buf_clear(&msg->optbuf);\n"
        "    for (i = 0; i < nelems; i++) {\n"
        "      ret = netstring_append_buf(&msg->optbuf, ptr[i].data,\n"
        "          ptr[i].len);\n"
        "      if (ret != NETSTRING_OK) {\n"
        "        goto done;\n"
        "      }\n"
        "    }\n"
        "    ret = netstring_append_pair(&msg->mbuf, \"%s\",\n"
        "        sizeof(\"%s\")-1, msg->optbuf.data, msg->optbuf.len);\n"
        "    if (ret != NETSTRING_OK) {\n"
        "      goto done;\n"
        "    }\n"
        "  }\n\n",
        f->name, f->name, f->name, f->name, f->name, f->name);
      if (ret <= 0) {
        return -1;
      }
      break;
    case FT_LONGARR:
    ret = fprintf(fp,
        "  if (r->%s != NULL && r->n%s > 0) {\n"
        "    long *ptr;\n"
        "    size_t i;\n"
        "    size_t nelems;\n"
        "    char numbuf[48];\n"
        "    char *num;\n"
        "\n"
        "    ptr = r->%s;\n"
        "    nelems = r->n%s;\n"
        "    buf_clear(&msg->optbuf);\n"
        "    for (i = 0; i < nelems; i++) {\n"
        "      num = l2s(numbuf, sizeof(numbuf), ptr[i]);\n"
        "      ret = netstring_append_buf(&msg->optbuf, num,\n"
        "        numbuf + sizeof(numbuf) - num);\n"
        "      if (ret != NETSTRING_OK) {\n"
        "        goto done;\n"
        "      }\n"
        "    }\n"
        "\n"
        "    ret = netstring_append_pair(&msg->mbuf, \"%s\",\n"
        "        sizeof(\"%s\")-1, msg->optbuf.data,\n"
        "        msg->optbuf.len);\n"
        "    if (ret != NETSTRING_OK) {\n"
        "      goto done;\n"
        "    }\n"
        "  }\n\n",
        f->name, f->name, f->name, f->name, f->name, f->name);
      break;
    }
  }

  ret = fprintf(fp,
    "  ret = netstring_append_buf(&msg->buf, msg->mbuf.data, msg->mbuf.len);\n"
    "  if (ret != NETSTRING_OK) {\n"
    "    goto done;\n"
    "  }\n"
    "\n"
    "  status = YCL_OK;\n"
    "done:\n"
    "  return status;\n"
    "}\n\n");
  if (ret <= 0) {
    return -1;
  }

  return 0;
}

static int write_parse_impl(FILE *fp, struct yclgen_msg *msg) {
  struct yclgen_field *f;
  int ret;
  int i;

  if (msg->nfields == 0) {
    ret = fprintf(fp,
        "int ycl_msg_parse_%s(struct ycl_msg *msg,\n"
        "    struct ycl_msg_%s *r) {\n"
        "  memset(r, 0, sizeof(*r));\n"
        "  return YCL_OK;\n"
        "}\n\n",
        msg->name, msg->name);
    if (ret <= 0) {
      return -1;
    }
    return 0;
  }

  ret = fprintf(fp,
      "int ycl_msg_parse_%s(struct ycl_msg *msg,\n"
      "    struct ycl_msg_%s *r) {\n"
      "  struct netstring_pair pair;\n"
      "  int result = YCL_ERR;\n"
      "  char *curr;\n"
      "  size_t len;\n"
      "  int ret;\n"
      "  size_t pairoff;\n"
      "  size_t currpair;\n"
      "  static const char *names[] = {\n",
      msg->name, msg->name);
  if (ret <= 0) {
    return -1;
  }

  for (i = 0; i < msg->nfields; i++) {
    ret = fprintf(fp, "      \"%s\",\n", msg->fields[i].name);
    if (ret <= 0) {
      return -1;
    }
  }

  ret = fprintf(fp,
      "  };\n"
      "\n"
      "  memset(r, 0, sizeof(*r));\n"
      "  buf_clear(&msg->mbuf);\n"
      "  ret = netstring_parse(&curr, &len, msg->buf.data, msg->buf.len);\n"
      "  if (ret != NETSTRING_OK) {\n"
      "    goto done;\n"
      "  }\n"
      "\n"
      "  /* ycl_msg_create_* will create messages in a determinable order.\n"
      "   * We use that order for our best case, and do a linear search for\n"
      "   * our worst case. */\n"
      "  for (pairoff = 0;\n"
      "      netstring_next_pair(&pair, &curr, &len) == NETSTRING_OK;\n"
      "      pairoff = (pairoff + 1) %% (sizeof(names) / sizeof(char*))) {\n"
      "    currpair = pairoff;\n"
      "    do {\n"
      "      if (strcmp(pair.key, names[currpair]) == 0) {\n"
      "        switch(currpair) {\n");
  if (ret <= 0) {
    return -1;
  }

  for (i = 0; i < msg->nfields; i++) {
    f = &msg->fields[i];
    ret = fprintf(fp, "        case %d: /* %s */\n", i, f->name);
    if (ret <= 0) {
      return -1;
    }

    switch (f->typ) {
    case FT_DATAARR:
      fprintf(fp,
          "          ret = load_datas(&msg->mbuf, pair.value, pair.valuelen,\n"
          "              (size_t*)&r->%s, &r->n%s);\n"
          "          if (ret != YCL_OK) {\n"
          "            goto done;\n"
          "          }\n",
          f->name, f->name);
      break;
    case FT_STRARR:
      fprintf(fp,
          "          ret = load_strs(&msg->mbuf, pair.value, pair.valuelen,\n"
          "              (size_t*)&r->%s, &r->n%s);\n"
          "          if (ret != YCL_OK) {\n"
          "            goto done;\n"
          "          }\n",
          f->name, f->name);
      break;
    case FT_LONGARR:
      fprintf(fp,
          "          ret = load_longs(&msg->mbuf, pair.value, pair.valuelen,\n"
          "              (size_t*)&r->%s, &r->n%s);\n"
          "          if (ret != YCL_OK) {\n"
          "            goto done;\n"
          "          }\n",
          f->name, f->name);
      break;
    case FT_DATA:
      fprintf(fp,
          "          r->%s.data = pair.value;\n"
          "          r->%s.len = pair.valuelen;\n",
          f->name, f->name);
      break;
    case FT_STR:
      fprintf(fp, "          r->%s = pair.value;\n", f->name);
      break;
    case FT_LONG:
      fprintf(fp, "          r->%s = s2l(pair.value);\n", f->name);
      break;
    }
    fprintf(fp, "          break;\n");
  }

  ret = fprintf(fp,
      "        default:\n"
      "          abort(); /* XXX: Inconsistent state */\n"
      "        }\n"
      "        break;\n"
      "      }\n"
      "      currpair = (currpair + 1) %% (sizeof(names) / sizeof(char*));\n"
      "    } while (currpair != pairoff);\n"
      "  }\n"
      "\n");
  if (ret <= 0) {
    return -1;
  }

  /* recalculate array addresses from offsets */
  for (i = 0; i < msg->nfields; i++) {
    f = &msg->fields[i];
    switch (f->typ) {
    case FT_DATAARR:
    case FT_STRARR:
    case FT_LONGARR:
      fprintf(fp,
          "  if (r->n%s > 0) {\n"
          "    r->%s = (void*)(msg->mbuf.data + (size_t)r->%s);\n"
          "  }\n",
          f->name, f->name, f->name);
      break;
    case FT_DATA:
    case FT_STR:
    case FT_LONG:
      break;
    }
  }

  ret = fprintf(fp, "\n"
      "  result = YCL_OK;\n"
      "done:\n"
      "  return result;\n"
      "}\n\n");
  if (ret <= 0) {
    return -1;
  }

  return 0;
}

static int write_impl(struct yclgen_ctx *ctx, FILE *fp, const char *hdrpath) {
  struct yclgen_msg *curr;
  int ret;
  struct yclgen_msg *msgs = ctx->latest_msg;

  /* write preamble */
  ret = fprintf(fp, "/* auto-generated by yclgen - DO NOT EDIT */\n"
      "#include <string.h>\n"
      "#include <stdio.h>\n"
      "#include <stdlib.h>\n"
      "#include <unistd.h>\n\n"
      "#include <lib/util/netstring.h>\n"
      "#include <%s>\n\n", hdrpath);
  if (ret <= 0) {
    return -1;
  }

  ret = write_static_impl(ctx, fp);
  if (ret < 0) {
    return -1;
  }

  for (curr = msgs; curr != NULL; curr = curr->next) {
    ret = write_create_impl(fp, curr);
    if (ret < 0) {
      return -1;
    }

    ret = write_parse_impl(fp, curr);
    if (ret < 0) {
      return -1;
    }
  }

  return 0;
}
static void usage(const char *name) {
  fprintf(stderr, "usage: %s <out-hdr> <out-impl>\n", name);
  exit(1);
}

int main(int argc, char *argv[]) {
  FILE *outhdr = NULL;
  FILE *outimpl = NULL;
  int status = EXIT_FAILURE;
  struct yclgen_ctx ctx = {0};
  int ret;

  if (argc != 3) {
    usage(argv[0]);
  }

  if (yclgen_parse(&ctx, stdin) != 0) {
    goto out;
  }

  outhdr = fopen(argv[1], "wb");
  if (outhdr == NULL) {
    perror(argv[1]);
    goto out;
  }

  ret = write_header(outhdr, ctx.latest_msg);
  if (ret < 0) {
    perror("write_header");
    goto out;
  }

  outimpl = fopen(argv[2], "wb");
  if (outimpl == NULL) {
    perror(argv[2]);
    goto out;
  }

  ret = write_impl(&ctx, outimpl, argv[1]);
  if (ret < 0) {
    perror("write_impl");
    goto out;
  }

  status = EXIT_SUCCESS;
out:
  yclgen_cleanup(&ctx);
  if (outhdr != NULL) {
    fclose(outhdr);
  }

  if (outimpl != NULL) {
    fclose(outimpl);
  }
  return status;
}

