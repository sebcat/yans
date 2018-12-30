#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <apps/grab-banners/bgrab.h>
#include <lib/util/sandbox.h>
#include <lib/util/zfile.h>

#define DFL_NCLIENTS    16 /* maxumum number of concurrent connections */
#define DFL_TIMEOUT      9 /* maximum connection lifetime, in seconds */
#define DFL_CONNECTS_PER_TICK 8
#define DFL_MDELAY_PER_TICK 500

/* command line options */
struct opts {
  struct bgrab_opts bgrab;
  char **dsts;
  int ndsts;
  int no_sandbox;
  int compress;          /* 1 if the output should be gzipped */
  const char *outpath;
};

static void print_bgrab_error(const char *err) {
  fprintf(stderr, "%s\n", err);
}

static void opts_or_die(struct opts *opts, int argc, char *argv[]) {
  int ch;
  const char *optstring = "n:t:Xc:d:o:z";
  const char *argv0;
  struct option optcfgs[] = {
    {"max-clients",       required_argument, NULL, 'n'},
    {"timeout",           required_argument, NULL, 't'},
    {"no-sandbox",        no_argument,       NULL, 'X'},
    {"connects-per-tick", required_argument, NULL, 'c'},
    {"mdelay-per-tick",   required_argument, NULL, 'd'},
    {"output-file",       required_argument, NULL, 'o'},
    {"compress",          no_argument,       NULL, 'z'},
    {NULL, 0, NULL, 0}
  };

  argv0 = argv[0];

  /* fill in defaults */
  opts->bgrab.max_clients = DFL_NCLIENTS;
  opts->bgrab.timeout = DFL_TIMEOUT;
  opts->bgrab.connects_per_tick = DFL_CONNECTS_PER_TICK;
  opts->bgrab.mdelay_per_tick = DFL_MDELAY_PER_TICK;
  opts->bgrab.on_error = print_bgrab_error;
  opts->outpath = NULL;
  opts->compress = 0;

  /* override defaults with command line arguments */
  while ((ch = getopt_long(argc, argv, optstring, optcfgs, NULL)) != -1) {
    switch(ch) {
    case 'n':
      opts->bgrab.max_clients = strtol(optarg, NULL, 10);
      break;
    case 't':
      opts->bgrab.timeout = strtol(optarg, NULL, 10);
      break;
    case 'X':
      opts->no_sandbox = 1;
      break;
    case 'c':
      opts->bgrab.connects_per_tick = strtol(optarg, NULL, 10);
      break;
    case 'd':
      opts->bgrab.mdelay_per_tick = strtol(optarg, NULL, 10);
      break;
    case 'o':
      opts->outpath = optarg;
      break;
    case 'z':
      opts->compress = 1;
      break;
    default:
      goto usage;
    }
  }

  argc -= optind;
  argv += optind;
  if (argc <= 0) {
    fprintf(stderr, "missing host/port pairs\n");
    goto usage;
  } else if (argc & 1) {
    fprintf(stderr, "uneven number of host/port elements\n");
    goto usage;
  }
  opts->dsts = argv;
  opts->ndsts = argc;

  if (opts->bgrab.max_clients <= 0) {
    fprintf(stderr, "unvalid max-clients value\n");
    goto usage;
  } else if (opts->bgrab.connects_per_tick <= 0) {
    fprintf(stderr, "connects-per-tick set too low\n");
    goto usage;
  } else if (opts->bgrab.connects_per_tick > opts->bgrab.max_clients) {
    fprintf(stderr, "connects-per-tick higher than max-clients\n");
    goto usage;
  }

  return;
usage:
  fprintf(stderr, "%s [opts] <hosts0> <ports0> .. [hostsN] [portsN]\n"
      "opts:\n"
      "  -n|--max-clients       <n>\n"
      "      # of max concurrent clients (%d)\n"
      "  -t|--timeout           <n>\n"
      "      connection lifetime in seconds (%d)\n"
      "  -X|--no-sandbox\n"
      "      disable sandbox\n"
      "  -c|--connects-per-tick <n>\n"
      "      # of conns per discretization (%d)\n"
      "  -d|--mdelay-per-tick   <n>\n"
      "      millisecond delay per tick (%d)\n"
      "  -o|--output-file       <path>\n"
      "      output file (stdout)\n"
      "  -z|--compress\n"
      "      compress output\n",
      argv0, DFL_NCLIENTS, DFL_TIMEOUT, DFL_CONNECTS_PER_TICK,
      DFL_MDELAY_PER_TICK);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  int i;
  int ret;
  struct opts opts = {0};
  struct bgrab_ctx grabber;
  int status = EXIT_FAILURE;
  struct tcpsrc_ctx tcpsrc;
  FILE *outfile = NULL;

  /* parse command line arguments to option struct */
  opts_or_die(&opts, argc, argv);

  /* ignore SIGPIPE, caused by writes on a file descriptor where the peer
   * has closed the connection */
  signal(SIGPIPE, SIG_IGN);

  /* initialice the TCP client connection source */
  ret = tcpsrc_init(&tcpsrc);
  if (ret != 0) {
    perror("tcpsrc_init");
    goto done;
  }

  /* set-up the input file */
  if (opts.outpath != NULL &&
      opts.outpath[0] != '-' &&
      opts.outpath[1] != '\0') {
    if (opts.compress) {
      outfile = zfile_open(opts.outpath, "wb");
    } else {
      outfile = fopen(opts.outpath, "wb");
    }
  } else if (opts.compress) {
    outfile = zfile_fdopen(STDOUT_FILENO, "wb");
  } else {
    outfile = stdout;
  }
  if (outfile == NULL) {
    if (errno == 0) {
      fprintf(stderr, "failed to open output file\n");
    } else {
      fprintf(stderr, "failed to open output file: %s\n", strerror(errno));
    }
    goto done_tcpsrc_cleanup;
  } else {
    opts.bgrab.outfile = outfile;
  }

  /* enter sandbox mode unless sandbox is disabled */
  if (opts.no_sandbox) {
    fprintf(stderr, "warning: sandbox disabled\n");
  } else {
    ret = sandbox_enter();
    if (ret != 0) {
      fprintf(stderr, "failed to enter sandbox mode\n");
      goto done_fclose_outfile;
    }
  }

  /* initialize the banner grabber */
  if (bgrab_init(&grabber, &opts.bgrab, tcpsrc) < 0) {
    perror("bgrab_init");
    goto done_fclose_outfile;
  }

  /* add the destinations to the banner grabber */
  for (i = 0; i < opts.ndsts / 2; i++) {
    ret = bgrab_add_dsts(&grabber, opts.dsts[i*2],
        opts.dsts[i*2+1], NULL);
    if (ret != 0) {
      fprintf(stderr, "bgrab_add_dsts: failed to add: %s %s\n",
          opts.dsts[i*2], opts.dsts[i*2+1]);
      goto done_bgrab_cleanup;
    }
  }

  /* Grab all the banners! */
  ret = bgrab_run(&grabber);
  if (ret < 0) {
    fprintf(stderr, "bgrab_run: failure (%s)\n", strerror(errno));
    goto done_bgrab_cleanup;
  }

  status = EXIT_SUCCESS;
done_bgrab_cleanup:
  bgrab_cleanup(&grabber);
done_fclose_outfile:
  if (outfile != stdout) {
    fclose(outfile);
  }
done_tcpsrc_cleanup:
  tcpsrc_cleanup(&tcpsrc);
done:
  return status;
}
