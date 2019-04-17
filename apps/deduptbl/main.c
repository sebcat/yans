#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <lib/util/deduptbl.h>
#include <lib/util/lines.h>
#include <lib/util/macros.h>
#include <lib/util/deduptbl.h>
#include <lib/ycl/opener.h>

#define YANS_ID_STR "YANS_ID"

static int run_lines(const char *socket, const char *dedup_path,
    const char *in, const char *out, unsigned int nentries) {
  int status = EXIT_FAILURE;
  struct lines_ctx linebuf;
  struct opener_ctx opener;
  FILE *infile;
  FILE *outfile;
  int fd = -1;
  int ret;
  int chunkret;
  int zlib = 0;
  char *line;
  size_t linelen;
  struct deduptbl_ctx deduptbl;
  struct deduptbl_id id;
  struct opener_opts opts = {
    .socket   = socket,
    .store_id = getenv(YANS_ID_STR),
  };

  ret = opener_init(&opener, &opts);
  if (ret < 0) {
    fprintf(stderr, "opener_init: %s\n", opener_strerror(&opener));
    return EXIT_FAILURE;
  }

  ret = opener_fopen(&opener, in, "rb", &infile);
  if (ret < 0) {
    fprintf(stderr, "%s: %s\n", in, opener_strerror(&opener));
    goto opener_cleanup;
  }

  ret = opener_fopen(&opener, out, "wb", &outfile);
  if (ret < 0) {
    fprintf(stderr, "%s: %s\n", in, opener_strerror(&opener));
    goto fclose_infile;
  }

  ret = lines_init(&linebuf, infile);
  if (ret != LINES_OK) {
    fprintf(stderr, "failed to set up input line buffer\n");
    goto fclose_outfile;
  }

  if (dedup_path) {
    ret = opener_open(&opener, dedup_path, O_RDWR, &zlib, &fd);
    if (ret < 0) {
      fprintf(stderr, "%s: %s\n", dedup_path, opener_strerror(&opener));
      goto lines_cleanup;
    } else if (zlib) {
      fprintf(stderr, "zlib compression for deduptbl is not supported\n");
      goto close_fd;
    }
  }

  if (fd >= 0) {
    ret = deduptbl_load(&deduptbl, fd);
    if (ret != DEDUPTBL_OK) {
      fprintf(stderr, "deduptbl_load: %s\n",
          deduptbl_strerror(&deduptbl, ret));
      goto close_fd;
    }
  } else {
    ret = deduptbl_create(&deduptbl, nentries, -1);
    if (ret != DEDUPTBL_OK) {
      fprintf(stderr, "deduptbl_create: %s\n",
          deduptbl_strerror(&deduptbl, ret));
      goto close_fd;
    }
  }

  while((chunkret = lines_next_chunk(&linebuf)) == LINES_CONTINUE) {
    while (lines_next(&linebuf, &line, &linelen) == LINES_CONTINUE) {
      deduptbl_id(&id, line, linelen);
      ret = deduptbl_update(&deduptbl, &id);
      if (ret == DEDUPTBL_OK) {
        ret = fwrite(line, 1, linelen, outfile);
        fputc('\n', outfile);
        if (ret < 0) {
          perror("fwrite");
          goto deduptbl_cleanup;
        }
      } else if (ret != DEDUPTBL_EEXIST) {
        fprintf(stderr, "deduptbl_update: %s\n",
            deduptbl_strerror(&deduptbl, ret));
        goto deduptbl_cleanup;
      }
    }
  }

  if (LINES_IS_ERR(chunkret)) {
    fprintf(stderr, "failed to read input chunk: %s\n",
        lines_strerror(chunkret));
    goto deduptbl_cleanup;
  }

  status = EXIT_SUCCESS;
deduptbl_cleanup:
  deduptbl_cleanup(&deduptbl);
close_fd:
  if (fd >= 0) {
    close(fd);
  }
lines_cleanup:
  lines_cleanup(&linebuf);
fclose_outfile:
  fclose(outfile);
fclose_infile:
  fclose(infile);
opener_cleanup:
  opener_cleanup(&opener);
  return status;
}

static int lines(int argc, char *argv[]) {
  const char *argv0 = argv[0];
  int ch;
  const char *out = "-";
  const char *in = "-";
  const char *dedup_path = NULL;
  const char *socket = STORECLI_DFLPATH;
  unsigned long nentries = 0;

  while ((ch = getopt(argc - 1, argv + 1, "f:i:o:s:n:")) != -1) {
    switch (ch) {
    case 'f':
      dedup_path = optarg;
      break;
    case 'i':
      in = optarg;
      break;
    case 'o':
      out = optarg;
      break;
    case 's':
      socket = optarg;
      break;
    case 'n':
      nentries = strtoul(optarg, NULL, 10);
      break;
    default:
      goto usage;
    }
  }

  /* TODO: Validate -f, -n values (min, max) and combinations */

  return run_lines(socket, dedup_path, in, out, (unsigned int)nentries);
usage:
  fprintf(stderr,
      "usage: %s lines [-s <sock>] [-f <dedup-path>] [-i <in>] [-o <out>]\n"
      "                [-n <nentries>]\n"
      , argv0);
  return EXIT_FAILURE;
}

static int create(int argc, char *argv[]) {
  int zlib = 0;
  int ch;
  int ret;
  int fd;
  int status = EXIT_FAILURE;
  unsigned long nentries = 0;
  struct deduptbl_ctx tbl;
  const char *argv0 = argv[0];
  struct opener_ctx opener;
  struct opener_opts opener_opts = {
    .socket = STORECLI_DFLPATH,
  };

  while ((ch = getopt(argc - 1, argv + 1, "n:s:c")) != -1) {
    switch (ch) {
    case 'n':
      nentries = strtoul(optarg, NULL, 10);
      break;
    case 's':
      opener_opts.socket = optarg;
      break;
    case 'c':
      opener_opts.flags = OPENER_FCREAT;
      break;
    default:
      goto usage;
    }
  }

  argc -= optind + 1;
  argv += optind + 1;
  if (argc != 1) {
    fprintf(stderr, "missing table path\n");
    goto usage;
  } else if (nentries == 0) {
    fprintf(stderr, "number of entries not set\n");
    goto usage;
  } else if (nentries > DEDUPTBL_MAX_ENTRIES) {
    fprintf(stderr, "number of entries too high\n");
    goto usage;
  }

  opener_opts.store_id = getenv(YANS_ID_STR);
  ret = opener_init(&opener, &opener_opts);
  if (ret < 0) {
    fprintf(stderr, "opener_init: %s\n", opener_strerror(&opener));
    return EXIT_FAILURE;
  }

  ret = opener_open(&opener, argv[0], O_RDWR | O_CREAT | O_TRUNC, &zlib,
      &fd);
  if (ret < 0) {
    fprintf(stderr, "opener_init: %s\n", opener_strerror(&opener));
    goto opener_cleanup;
  } else if (zlib) {
    fprintf(stderr, "zlib compression for deduptbl is not supported\n");
    goto close_fd;
  }

  ret = deduptbl_create(&tbl, (uint32_t)nentries, fd);
  if (ret != DEDUPTBL_OK) {
    fprintf(stderr, "deduptbl_create: %s\n", deduptbl_strerror(&tbl, ret));
    goto close_fd;
  }

  if (opener_opts.flags & OPENER_FCREAT) {
    printf("%s\n", opener_store_id(&opener));
  }

  status = EXIT_SUCCESS;
close_fd:
  close(fd);
opener_cleanup:
  opener_cleanup(&opener);
  return status;

usage:
  fprintf(stderr,
      "usage: %s create [-c] [-s <socket>] [-n <nentries>] <path>\n"
      , argv0);
  return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
  size_t i;
  static const struct {
    const char *name;
    int (*func)(int, char **);
    const char *desc;
  } cmds[] = {
    {"lines", lines, "Dedup a sequence of newline terminated strings"},
    {"create", create, "Create a deduptbl on disk"},
  };

  if (argc < 2) {
    goto usage;
  }

  for (i = 0; i < ARRAY_SIZE(cmds); i++) {
    if (strcmp(cmds[i].name, argv[1]) == 0) {
      return cmds[i].func(argc, argv);
    }
  }

usage:
  fprintf(stderr, "usage: %s <cmd> [args]\nCommands:\n", argv[0]);
  for (i = 0; i < ARRAY_SIZE(cmds); i++) {
    fprintf(stderr, "  %s\n    %s\n", cmds[i].name, cmds[i].desc);
  }

  return EXIT_FAILURE;
}
