#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>

#define MAX_MODULES 8 /* 8 modules should be enough for everybody! */

struct module_data {
  char *name;
  int argc;
  char **argv;

  int (*mod_init)(int, char **, void **);
  /* TODO: mod_process */
};

struct opts {
  int nmodules;
  struct module_data modules[MAX_MODULES];
};


static void opts_or_die(struct opts *opts, int argc, char **argv) {
  int modoff = 0;
  int paramoff = 0;
  int pos;
  int ch;
  int i;
  const char *optstr = "m:ph";
  static const struct option options[] = {
    {"--module",      required_argument, NULL, 'm'},
    {"--params",      no_argument,       NULL, 'p'},
    {"--help",        no_argument,       NULL, 'h'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc, argv, optstr, options, NULL)) != -1) {
    switch(ch) {
    case 'm':
      if (modoff >= MAX_MODULES) {
        fprintf(stderr, "too many modules given\n");
        goto usage;
      }
      opts->modules[modoff++].name = optarg;
      opts->nmodules = modoff;
      break;
    case 'p':
      if (paramoff >= MAX_MODULES) {
        fprintf(stderr, "too many params given\n");
        goto usage;
      }

      /* find the end of the params */
      for (pos = optind; pos < argc && strcmp(argv[pos], ";"); pos++);
      pos++;

      /* set the argv, argc of the module to that of the command line.
       * include the -p/--params flag for later replacement of argv[0] */
      opts->modules[paramoff].argc = pos - optind;
      opts->modules[paramoff].argv = argv + optind - 1;
      paramoff++;
      optind = pos;
      break;
    case 'h':
    default:
      goto usage;
    }
  }

  /* update the module argument vector with module name as argv[0] */
  for (i = 0; i < opts->nmodules; i++) {
    if (opts->modules[i].argv) {
      opts->modules[i].argv[0] = opts->modules[i].name;
    }
  }

  for (pos = 0; pos < opts->nmodules; pos++) {
    printf("%s: ", opts->modules[pos].name);
    for (ch = 0; ch < opts->modules[pos].argc; ch++) {
      fprintf(stdout, "%s ", opts->modules[pos].argv[ch]);
    }
    printf(" (%d)\n", opts->modules[pos].argc);
  }

  return;
usage:
  fprintf(stderr, "usage: %s [opts]\n", argv[0]);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  int status = EXIT_FAILURE;
  static struct opts opts;

  opts_or_die(&opts, argc, argv);
  signal(SIGPIPE, SIG_IGN);

  status = EXIT_SUCCESS;
/* done:*/
  return status;
}
