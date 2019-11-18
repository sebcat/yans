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
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>

#include <lib/util/sandbox.h>
#include <lib/net/tcpsrc.h>
#include <lib/ycl/opener.h>
#include <lib/ycl/ycl_msg.h>

#include <curl/curl.h>

#include <apps/webfetch/fetch.h>
#include <apps/webfetch/module.h>

#define MAX_MODULES 8 /* 8 modules should be enough for everybody! */

#define MAX_NFETCHERS     100
#define DEFAULT_NFETCHERS   8

struct opts {
  int sandbox;
  int nfetchers;
  int maxsize;
  const char *opener_socket;
  const char *input_path; /* TODO: We could have multiple input paths */
  int nmodules;
  struct module_data modules[MAX_MODULES];
};

static void on_completed(struct fetch_transfer *t, void *data) {
  struct opts *opts;
  struct module_data *mod;
  int i;

  opts = data;
  for (i = 0; i < opts->nmodules; i++) {
    mod = &opts->modules[i];
    if (mod->mod_process != NULL) {
      mod->mod_process(t, mod->mod_data);
    }
  }
}

static void opts_or_die(struct opts *opts, int argc, char **argv) {
  int pos;
  int ch;
  int i;
  const char *optstr = "m:pXs:i:n:c:h";
  static const struct option options[] = {
    {"module",      required_argument, NULL, 'm'},
    {"params",      no_argument,       NULL, 'p'},
    {"no-sandbox",  no_argument,       NULL, 'X'},
    {"opener-sock", required_argument, NULL, 's'},
    {"in-httpmsgs", required_argument, NULL, 'i'},
    {"nfetchers",   required_argument, NULL, 'n'},
    {"maxsize",     required_argument, NULL, 'c'},
    {"help",        no_argument,       NULL, 'h'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc, argv, optstr, options, NULL)) != -1) {
    switch(ch) {
    case 'm':
      if (opts->modules[opts->nmodules].name) {
        opts->nmodules++;
      }

      if (opts->nmodules >= MAX_MODULES) {
        fprintf(stderr, "too many modules given\n");
        goto usage;
      }
      opts->modules[opts->nmodules].name = optarg;
      break;
    case 'p':
      /* make sure we have a module set up */
      if (!opts->modules[opts->nmodules].name) {
        fprintf(stderr, "param directive must follow a module\n");
      }

      /* find the end of the params */
      for (pos = optind; pos < argc && strcmp(argv[pos], ";"); pos++);
      pos++;

      /* set the argv, argc of the module to that of the command line.
       * include the -p/--params flag for later replacement of argv[0] */
      opts->modules[opts->nmodules].argc = pos - optind;
      opts->modules[opts->nmodules].argv = argv + optind - 1;
      optind = pos;
      opts->nmodules++;
      break;
    case 'X':
      opts->sandbox = 0;
      break;
    case 's':
      opts->opener_socket = optarg;
      break;
    case 'i':
      opts->input_path = optarg;
      break;
    case 'n':
      opts->nfetchers = (int)strtol(optarg, NULL, 10);
      break;
    case 'c':
      opts->maxsize = (int)strtol(optarg, NULL, 10);
      break;
    case 'h':
    default:
      goto usage;
    }
  }

  if (opts->nfetchers < 0 || opts->nfetchers >= MAX_NFETCHERS) {
    fprintf(stderr, "invalid number of fetchers\n");
    goto usage;
  }

  /* if we stopped on a -m, we need to advance opts->nmodules here */
  if (opts->modules[opts->nmodules].name) {
    opts->nmodules++;
  }

  return;
usage:
  fprintf(stderr, "usage: %s [opts]\nopts:\n", argv[0]);
  for (i = 0; options[i].name != NULL; i++) {
    printf("  -%c|--%s\n", options[i].val, options[i].name);
  }

  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  int status = EXIT_FAILURE;
  int ret;
  int i;
  struct opener_opts opener_opts = {0};
  struct opener_ctx opener;
  struct tcpsrc_ctx tcpsrc;
  struct fetch_ctx fetch;
  struct fetch_opts fetch_opts = {0};
  struct module_data *mod;
  FILE *infp;
  static struct opts opts = {
    /* default values, possibly overridden in opts_or_die */
    .sandbox    = 1,
    .input_path = "-",
    .nfetchers  = DEFAULT_NFETCHERS,
  };

  opts_or_die(&opts, argc, argv);
  signal(SIGPIPE, SIG_IGN);
  curl_global_init(CURL_GLOBAL_ALL);

  /* Init the TCP connection source */
  ret = tcpsrc_init(&tcpsrc);
  if (ret < 0) {
    perror("tcpsrc_init");
    goto done;
  }

  /* Init the file opener */
  opener_opts.store_id = getenv("YANS_ID");
  opener_opts.socket = opts.opener_socket;
  ret = opener_init(&opener, &opener_opts);
  if (ret < 0) {
    fprintf(stderr, "opener_init: %s\n", opener_strerror(&opener));
    goto tcpsrc_cleanup;
  }

  /* enter the sandbox, unless told otherwise at the command line */
  if (opts.sandbox) {
    ret = sandbox_enter();
    if (ret < 0) {
      fprintf(stderr, "sandbox_enter failure\n");
      goto opener_cleanup;
    }
  } else {
    fprintf(stderr, "warning: sandbox disabled\n");
  }

  /* Init the modules, do this after entering the sandbox */
  for (i = 0; i < opts.nmodules; i++) {
    mod = &opts.modules[i];

    /* update the module argument vector (if any) with module name as
     * argv[0] */
    if (mod->argv) {
      mod->argv[0] = mod->name;
    } else {
      mod->argv = &mod->name;
      mod->argc = 1;
    }

    /* make the module aware of the opener */
    mod->opener = &opener;

    /* load the module */
    ret = module_load(mod);
    if (ret < 0) {
      fprintf(stderr, "failed to load module %s\n", mod->name);
      goto module_cleanup;
    }
  }

  /* open the input file for HTTP messages */
  ret = opener_fopen(&opener, opts.input_path, "rb", &infp);
  if (ret < 0) {
    fprintf(stderr, "opener_init: %s\n", opener_strerror(&opener));
    goto module_cleanup;
  }

  /* initialize the fetcher */
  fetch_opts.infp          = infp;
  fetch_opts.tcpsrc        = &tcpsrc;
  fetch_opts.nfetchers     = opts.nfetchers;
  fetch_opts.maxsize       = opts.maxsize;
  fetch_opts.on_completed  = on_completed;
  fetch_opts.completeddata = &opts;
  ret = fetch_init(&fetch, &fetch_opts);
  if (ret < 0) {
    fprintf(stderr, "fetch_init failure\n");
    goto fclose_infp;
  }

  /* fetch all the requests! */
  fetch_run(&fetch);

  status = EXIT_SUCCESS;
/* fetch_cleanup: */
  fetch_cleanup(&fetch);
fclose_infp:
  fclose(infp);
module_cleanup:
  for (i = 0; i < opts.nmodules; i++) {
    module_cleanup(&opts.modules[i]);
  }
opener_cleanup:
  opener_cleanup(&opener);
tcpsrc_cleanup:
  tcpsrc_cleanup(&tcpsrc);
done:
  return status;
}
