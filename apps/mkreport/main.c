#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>

#include <apps/mkreport/services.h>

static const struct report_types types_[] = {
  {"services", services_report},
};

int main(int argc, char *argv[]) {
  struct report_opts opts = {0};
  const char *argv0 = argv[0];
  size_t i;
  int ch;
  const char *optstr = "t:o:";
  const char *typestr = NULL;
  static const struct option getopts[] = {
    {"type",   required_argument, NULL, 't'},
    {"output", required_argument, NULL, 'o'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc, argv, optstr, getopts, NULL)) != -1) {
    switch (ch) {
    case 't':
      typestr = optarg;
      break;
    case 'o':
      opts.output = optarg;
      break;
    default:
      fprintf(stderr, "unknown option: -%c\n", ch);
      goto usage;
    }
  }

  opts.inputs = argv + optind;
  opts.ninputs = argc - optind;

  if (opts.ninputs <= 0) {
    fprintf(stderr, "no inputs given\n");
    goto usage;
  }

  for (i = 0; typestr && i < sizeof(types_) / sizeof(*types_); i++) {
    if (strcmp(types_[i].name, typestr) == 0) {
      return types_[i].func(&opts);
    }
  }

  fprintf(stderr, "missing or invalid type\n");
usage:
  fprintf(stderr, "usage: %s <opts> <input0> ... [inputN]\n"
      "opts:\n"
      "  -t|--type   <t>     type of report to generate\n"
      "  -o|--output <name>  name of output file\n"
      , argv0);

  fprintf(stderr, "report types:\n");
  for (i = 0; i < sizeof(types_) / sizeof(*types_); i++) {
    fprintf(stderr, "  %s\n", types_[i].name);
  }

  return EXIT_FAILURE;
}
