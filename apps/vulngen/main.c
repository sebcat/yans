#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "lib/vulnspec/vulnspec.h"

static void die(const char *msg) {
  fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  int ch;
  FILE *infp = stdin;
  FILE *outfp = stdout;
  int ret;
  struct vulnspec_parser p;
  int status = EXIT_FAILURE;
  size_t len;
  const char *data;

  while ((ch = getopt(argc, argv, "hf:o:")) != -1) {
    switch(ch) {
    case 'f':
      infp = fopen(optarg, "rb");
      if (!infp) {
        perror(optarg);
        exit(EXIT_FAILURE);
      }
      break;
    case 'o':
      outfp = fopen(optarg, "wb");
      if (!infp) {
        perror(optarg);
        exit(EXIT_FAILURE);
      }
      break;
    case 'h':
    default:
      die("usage: vulngen [-f <infile>] [-o <outfile>] [-h]");
      break;
    }
  }

  if (isatty(fileno(outfp))) {
    die("refusing output to tty");
  }

  ret = vulnspec_parser_init(&p);
  if (ret != 0) {
    perror("vulnspec_parser");
    goto done;
  }

  ret = vulnspec_parse(&p, infp);
  if (ret != 0) {
    fprintf(stderr, "vulnspec_parse failure\n");
    goto vulnspec_parser_cleanup;
  }

  data = vulnspec_parser_data(&p, &len);
  if (fwrite(data, 1, len, outfp) != len) {
    perror("fwrite");
    goto vulnspec_parser_cleanup;
  }

  status = EXIT_SUCCESS;
vulnspec_parser_cleanup:
  vulnspec_parser_cleanup(&p);
done:
  fclose(infp);
  fclose(outfp);
  return status;
}
