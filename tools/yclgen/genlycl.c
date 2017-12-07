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

void emit_header(struct yclgen_ctx *ctx, FILE *out) {

}

void emit_impl(struct yclgen_ctx *ctx, FILE *out) {

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
    if (*linebuf != '@') {
      fputs(linebuf, stdout);
      continue;
    }

    if (strcmp(linebuf, "@@YCLHDR@@") == 0) {
      emit_header(&ctx, stdout);
    } else if (strcmp(linebuf, "@@YCLIMPL@@") == 0) {
      emit_impl(&ctx, stdout);
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
