#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "lib/vulnmatch/vulnmatch.h"

static void die(const char *msg) {
  fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  int ch;
  FILE *infp = stdin;
  FILE *outfp = stdout;
  int ret;
  struct vulnmatch_parser p;
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

  ret = vulnmatch_parser_init(&p);
  if (ret != 0) {
    perror("vulnmatch_parser");
    goto done;
  }

  ret = vulnmatch_parse(&p, infp);
  if (ret != 0) {
    fprintf(stderr, "vulnmatch_parse failure\n");
    goto vulnmatch_parser_cleanup;
  }

  data = vulnmatch_parser_data(&p, &len);
  if (fwrite(data, 1, len, outfp) != len) {
    perror("fwrite");
    goto vulnmatch_parser_cleanup;
  }

  status = EXIT_SUCCESS;
vulnmatch_parser_cleanup:
  vulnmatch_parser_cleanup(&p);
done:
  fclose(infp);
  fclose(outfp);
  return status;
}
