#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <openssl/ssl.h>

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
  int no_sandbox;        /* 1 if the sandbox should be disabled */
  int compress;          /* 1 if the output should be gzipped */
  int tls;               /* 1 if TLS should be used */
  const char *outpath;
  char *hostname;
};

static void print_bgrab_error(const char *err) {
  fprintf(stderr, "%s\n", err);
}

static void opts_or_die(struct opts *opts, int argc, char *argv[]) {
  int ch;
  const char *optstring = "n:t:Xc:d:o:zsH:";
  const char *argv0;
  struct option optcfgs[] = {
    {"max-clients",       required_argument, NULL, 'n'},
    {"timeout",           required_argument, NULL, 't'},
    {"no-sandbox",        no_argument,       NULL, 'X'},
    {"connects-per-tick", required_argument, NULL, 'c'},
    {"mdelay-per-tick",   required_argument, NULL, 'd'},
    {"output-file",       required_argument, NULL, 'o'},
    {"compress",          no_argument,       NULL, 'z'},
    {"tls",               no_argument,       NULL, 's'},
    {"hostname",          required_argument, NULL, 'H'},
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
  opts->tls = 0;
  opts->hostname = NULL;

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
    case 's':
      opts->tls = 1;
      break;
    case 'H':
      opts->hostname = optarg;
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

  return;
usage:
  fprintf(stderr, "%s [opts] <hosts0> <ports0> .. [hostsN] [portsN]\n"
      "opts:\n"
      "  -n|--max-clients <n>\n"
      "      Maximum number of concurrent clients (%d)\n"
      "  -t|--timeout <n>\n"
      "      Connection lifetime, in seconds (%d)\n"
      "  -X|--no-sandbox\n"
      "      Disable sandbox\n"
      "  -c|--connects-per-tick <n>\n"
      "      Number of connections per time discretization (%d)\n"
      "  -d|--mdelay-per-tick <n>\n"
      "      Millisecond delay per tick (%d)\n"
      "  -o|--output-file <path>\n"
      "      Output file (stdout)\n"
      "  -z|--compress\n"
      "      Compress output\n"
      "  -s|--tls\n"
      "      Use TLS\n"
      "  -H|--hostname <name>\n"
      "      Hostname to use, if any\n",
      argv0, DFL_NCLIENTS, DFL_TIMEOUT, DFL_CONNECTS_PER_TICK,
      DFL_MDELAY_PER_TICK);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  int i;
  int ret;
  struct opts opts = {0};
  struct bgrab_ctx grabber;
  struct tcpsrc_ctx tcpsrc;
  int status = EXIT_FAILURE;
  FILE *outfile = NULL;
  SSL_CTX *ssl_ctx = NULL;

  /* parse command line arguments to option struct */
  opts_or_die(&opts, argc, argv);

  /* if TLS is to be used, initiate the library and allocate the TLS
   * context */
  if (opts.tls) {
    SSL_library_init();
    ssl_ctx = SSL_CTX_new(SSLv23_client_method());
    if (ssl_ctx == NULL) {
      fprintf(stderr, "failed to initialize the TLS context\n");
      return EXIT_FAILURE;
    }
    /* TODO: Implement --(no-)verify flag by setting SSL_VERIFY_NONE
     *       using SSL_CTX_set_verify here */
  }

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
  opts.bgrab.outfile = outfile;
  opts.bgrab.ssl_ctx = ssl_ctx;
  if (bgrab_init(&grabber, &opts.bgrab, tcpsrc) < 0) {
    fprintf(stderr, "bgrab_init: %s\n", bgrab_strerror(&grabber));
    goto done_fclose_outfile;
  }

  /* add the destinations to the banner grabber */
  for (i = 0; i < opts.ndsts / 2; i++) {
    ret = bgrab_add_dsts(&grabber, opts.dsts[i*2],
        opts.dsts[i*2+1], opts.hostname);
    if (ret != 0) {
      fprintf(stderr, "bgrab_add_dsts: failed to add: %s %s\n",
          opts.dsts[i*2], opts.dsts[i*2+1]);
      goto done_bgrab_cleanup;
    }
  }

  /* Grab all the banners! */
  ret = bgrab_run(&grabber);
  if (ret < 0) {
    fprintf(stderr, "bgrab_run: %s\n", bgrab_strerror(&grabber));
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
  if (ssl_ctx != NULL) {
    SSL_CTX_free(ssl_ctx);
  }

  return status;
}
