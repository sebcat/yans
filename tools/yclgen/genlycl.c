#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "yclgen.h"

struct opts {
  const char *defsfile;
};

static void usage() {
  fprintf(stderr,
      "opts:\n"
      "  -f <file>   - ycl definitions file\n"
      "  -h          - this text\n");
  exit(EXIT_FAILURE);
}

static void opts_or_die(struct opts *opts, int argc, char **argv) {
  int ch;

  while ((ch = getopt(argc, argv, "f:h")) != -1) {
    switch (ch) {
    case 'f':
      opts->defsfile = optarg;
      break;
    case 'h':
    default:
      usage();
    }
  }

  if (opts->defsfile == NULL) {
    fprintf(stderr, "ycl definitions not set\n");
    usage();
  }
}

static int load_yclgen_ctx(struct yclgen_ctx *ctx, struct opts *opts) {
  FILE *fp;
  int ret;

  fp = fopen(opts->defsfile, "rb");
  if (fp == NULL) {
    perror(opts->defsfile);
    return -1;
  }

  ret = yclgen_parse(ctx, fp);
  fclose(fp);
  return ret;
}

static const char *get_msg_initializer(struct yclgen_msg *msg) {
  const char *initializer = NULL;

  if (msg->nfields == 0) {
    initializer = "";
  } else if (msg->flags == MSGF_HASDATA) {
    /* certain versions of clang complains about missing braces if something
     * just contains nested structs and is initialized with single braces,
     * or I'm misinterpreting the error messages */
    initializer = " = {{0}}";
  } else {
    initializer = " = {0}";
  }

  return initializer;
}

void emit_create_impl(struct yclgen_msg *msg, FILE *out) {
  int i;
  struct yclgen_field *f;

  fprintf(out,
      "static int l_msgcreate%s(lua_State *L) {\n"
      "  struct ycl_msg_%s mdata%s;\n"
      "  struct ycl_msg *m;\n"
      "  int ret;\n"
      "%s"
      "\n"
      "  m = checkmsg(L, 1);\n"
      "  luaL_checktype(L, 2, LUA_TTABLE);\n"
      "\n",
      msg->name, msg->name,
      get_msg_initializer(msg),
      msg->nfields > 0 ? "  int t;\n" : "");

  for (i = 0; i < msg->nfields; i++) {
    f = &msg->fields[i];
    switch (f->typ) {
    case FT_DATAARR:
      fprintf(out,
          "  t = lua_getfield(L, 2, \"%s\");\n"
          "  if (t == LUA_TTABLE) {\n"
          "    size_t len;\n"
          "    size_t i;\n"
          "\n"
          "    len = lua_rawlen(L, -1);\n"
          "    if (len > 0 && (mdata.%s = malloc(sizeof(struct ycl_data) * len))) {\n"
          "      for (i = 0; i < len && i < INT_MAX; i++) {\n"
          "        lua_rawgeti(L, -1, (lua_Integer)i + 1);\n"
          "        mdata.%s[i].data = lua_tolstring(L, -1, &mdata.%s[i].len);\n"
          "        lua_pop(L, 1);\n"
          "      }\n"
          "      mdata.n%s = len;\n"
          "    }\n"
          "  }\n"
          "  lua_pop(L, 1);\n"
          "\n", f->name, f->name, f->name, f->name, f->name);
      break;
    case FT_STRARR:
      fprintf(out,
          "  t = lua_getfield(L, 2, \"%s\");\n"
          "  if (t == LUA_TTABLE) {\n"
          "    size_t len;\n"
          "    size_t i;\n"
          "\n"
          "    len = lua_rawlen(L, -1);\n"
          "    if (len > 0 && (mdata.%s = malloc(sizeof(char *) * len))) {\n"
          "      for (i = 0; i < len && i < INT_MAX; i++) {\n"
          "        lua_rawgeti(L, -1, (lua_Integer)i + 1);\n"
          "        mdata.%s[i] = lua_tostring(L, -1);\n"
          "        lua_pop(L, 1);\n"
          "      }\n"
          "      mdata.n%s = len;\n"
          "    }\n"
          "  }\n"
          "  lua_pop(L, 1);\n"
          "\n", f->name, f->name, f->name, f->name);
      break;
    case FT_LONGARR:
      fprintf(out,
          "  t = lua_getfield(L, 2, \"%s\");\n"
          "  if (t == LUA_TTABLE) {\n"
          "    size_t len;\n"
          "    size_t i;\n"
          "\n"
          "    len = lua_rawlen(L, -1);\n"
          "    if (len > 0 && (mdata.%s = malloc(sizeof(long) * len))) {\n"
          "      for (i = 0; i < len && i < INT_MAX; i++) {\n"
          "        lua_rawgeti(L, -1, (lua_Integer)i + 1);\n"
          "        mdata.%s[i] = (long)lua_tonumber(L, -1);\n"
          "        lua_pop(L, 1);\n"
          "      }\n"
          "      mdata.n%s = len;\n"
          "    }\n"
          "  }\n"
          "  lua_pop(L, 1);\n"
          "\n", f->name, f->name, f->name, f->name);
      break;
    case FT_DATA:
      fprintf(out,
          "  t = lua_getfield(L, 2, \"%s\");\n"
          "  if (t == LUA_TSTRING) {\n"
          "    mdata.%s.data = lua_tolstring(L, -1, &mdata.%s.len);\n"
          "  }\n"
          "  lua_pop(L, 1);\n"
          "\n", f->name, f->name, f->name);
      break;
    case FT_STR:
      fprintf(out,
          "  t = lua_getfield(L, 2, \"%s\");\n"
          "  if (t == LUA_TSTRING) {\n"
          "    mdata.%s = lua_tostring(L, -1);\n"
          "  }\n"
          "  lua_pop(L, 1);\n"
          "\n", f->name, f->name);
      break;
    case FT_LONG:
      fprintf(out,
          "  t = lua_getfield(L, 2, \"%s\");\n"
          "  if (t == LUA_TNUMBER) {\n"
          "    mdata.%s = (long)lua_tointeger(L, -1);\n"
          "  }\n"
          "  lua_pop(L, 1);\n"
          "\n", f->name, f->name);
      break;
    default:
      abort();
      break;
    }
  }

  fprintf(out, "  ret = ycl_msg_create_%s(m, &mdata);\n", msg->name);
  for (i = 0; i < msg->nfields; i++) {
    f = &msg->fields[i];
    switch (f->typ) {
    case FT_DATAARR:
    case FT_STRARR:
    case FT_LONGARR:
      fprintf(out,
          "  if (mdata.%s != NULL) {\n"
          "    free(mdata.%s);\n"
          "  }\n\n", f->name, f->name);
      break;
    case FT_DATA:
    case FT_STR:
    case FT_LONG:
    default:
      /* no-op */
      break;
    }
  }

  fprintf(out,
      "  if (ret != YCL_OK) {\n"
      "    return luaL_error(L, \"ycl_msg_create_%s failure\");\n"
      "  }\n"
      "\n"
      "  lua_pop(L, 1); /* pop arg table, return self */\n"
      "  return 1;\n"
      "}\n\n", msg->name);
}

void emit_parse_impl(struct yclgen_msg *msg, FILE *out) {
  struct yclgen_field *f;
  int i;

  fprintf(out,
      "static int l_msgparse%s(lua_State *L) {\n"
      "  struct ycl_msg *m;\n"
      "  struct ycl_msg_%s mdata%s;\n"
      "  int ret;\n"
      "\n"
      "  m = checkmsg(L, 1);\n"
      "  ret = ycl_msg_parse_%s(m, &mdata);\n"
      "  if (ret != YCL_OK) {\n"
      "    return luaL_error(L, \"ycl_msg_parse_%s failure\");\n"
      "  }\n"
      "\n"
      "  lua_createtable(L, 0, %d);\n"
      "\n", msg->name, msg->name,
      get_msg_initializer(msg),
      msg->name, msg->name, msg->nfields);

  for (i = 0; i < msg->nfields; i++) {
    f = &msg->fields[i];
    switch (f->typ) {
    case FT_DATA:
      fprintf(out,
          "  if (mdata.%s.len > 0) {\n"
          "    lua_pushlstring(L, mdata.%s.data, mdata.%s.len);\n"
          "    lua_setfield(L, -2, \"%s\");\n"
          "  }\n"
          "\n", f->name, f->name, f->name, f->name);
      break;
    case FT_STR:
      fprintf(out,
          "  if (mdata.%s != NULL) {\n"
          "    lua_pushstring(L, mdata.%s);\n"
          "    lua_setfield(L, -2, \"%s\");\n"
          "  }\n"
          "\n", f->name, f->name, f->name);
      break;
    case FT_LONG:
      fprintf(out,
          "  lua_pushinteger(L, (lua_Integer)mdata.%s);\n"
          "  lua_setfield(L, -2, \"%s\");\n"
          "\n", f->name, f->name);
      break;
    case FT_DATAARR:
      fprintf(out,
          "  if (mdata.%s != NULL) {\n"
          "    size_t i;\n"
          "\n"
          "    lua_createtable(L, mdata.n%s, 0);\n"
          "    for (i = 0; i < mdata.n%s; i++) {\n"
          "      lua_pushlstring(L, mdata.%s[i].data, mdata.%s[i].len);\n"
          "      lua_rawseti(L, -2, (lua_Integer)i + 1);\n"
          "    }\n"
          "    lua_setfield(L, -2, \"%s\");\n"
          "  }\n"
          "\n", f->name, f->name, f->name, f->name, f->name, f->name);
      break;
    case FT_STRARR:
      fprintf(out,
          "  if (mdata.%s != NULL) {\n"
          "    size_t i;\n"
          "\n"
          "    lua_createtable(L, mdata.n%s, 0);\n"
          "    for (i = 0; i < mdata.n%s; i++) {\n"
          "      lua_pushstring(L, mdata.%s[i]);\n"
          "      lua_rawseti(L, -2, (lua_Integer)i + 1);\n"
          "    }\n"
          "    lua_setfield(L, -2, \"%s\");\n"
          "  }\n"
          "\n", f->name, f->name, f->name, f->name, f->name);
      break;
    case FT_LONGARR:
      fprintf(out,
          "  if (mdata.%s != NULL) {\n"
          "    size_t i;\n"
          "\n"
          "    lua_createtable(L, mdata.n%s, 0);\n"
          "    for (i = 0; i < mdata.n%s; i++) {\n"
          "      lua_pushinteger(L, (lua_Integer)mdata.%s[i]);\n"
          "      lua_rawseti(L, -2, (lua_Integer)i + 1);\n"
          "    }\n"
          "    lua_setfield(L, -2, \"%s\");\n"
          "  }\n"
          "\n", f->name, f->name, f->name, f->name, f->name);
      break;
    default:
      abort();
      break;
    }
  }

  fprintf(out,
      "  return 1;\n"
      "}\n"
      "\n");
}

void emit_impl(struct yclgen_ctx *ctx, FILE *out) {
  struct yclgen_msg *curr;

  for (curr = ctx->latest_msg; curr != NULL; curr = curr->next) {
    emit_create_impl(curr, out);
    emit_parse_impl(curr, out);
  }
}

void emit_table(struct yclgen_ctx *ctx, FILE *out) {
  struct yclgen_msg *curr;

  for (curr = ctx->latest_msg; curr != NULL; curr = curr->next) {
    fprintf(out,
        "  {\"create_%s\", l_msgcreate%s},\n"
        "  {\"parse_%s\", l_msgparse%s},\n",
        curr->name, curr->name, curr->name, curr->name);
  }
}

int main(int argc, char *argv[]) {
  struct opts opts = {0};
  struct yclgen_ctx ctx = {0};
  int status = EXIT_FAILURE;
  char linebuf[256];

  opts_or_die(&opts, argc, argv);
  if (load_yclgen_ctx(&ctx, &opts) != 0) {
    goto out;
  }

  while (fgets(linebuf, sizeof(linebuf), stdin) != NULL) {
    /* fast-path: write line to stdout */
    if (*linebuf != '/') {
      fputs(linebuf, stdout);
      continue;
    }

    if (strcmp(linebuf, "//@@YCLIMPL@@\n") == 0) {
      emit_impl(&ctx, stdout);
    } else if (strcmp(linebuf, "//@@YCLTBL@@\n") == 0) {
      emit_table(&ctx, stdout);
    } else {
      fputs(linebuf, stdout);
    }
  }

  if (!feof(stdin) || ferror(stdin)) {
    perror("read error");
    goto out;
  }

  if (ferror(stdout)) {
    perror("write error");
    goto out;
  }

  status = EXIT_SUCCESS;
out:
  yclgen_cleanup(&ctx);
  return status;
}
