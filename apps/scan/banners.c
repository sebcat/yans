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
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <openssl/ssl.h>

#include <lib/util/lines.h>
#include <lib/util/sandbox.h>
#include <lib/util/str.h>
#include <lib/util/zfile.h>
#include <apps/scan/bgrab.h>
#include <apps/scan/banners.h>

#define DFL_NCLIENTS    16 /* maxumum number of concurrent connections */
#define DFL_TIMEOUT      9 /* maximum connection lifetime, in seconds */
#define DFL_CONNECTS_PER_TICK 8
#define DFL_MDELAY_PER_TICK 500
#define DFL_INPATH     "-"
#define DFL_OUTPATH    "-"

struct input_line {
  char *parts[2];
  size_t lens[2];
  unsigned int i;
};

static int mapfunc(const char *s, size_t len, void *data) {
  struct input_line *input = data;
  unsigned int i;

  i = input->i;

  if (i > 1) {
    return 0;
  }

  input->parts[i] = (char*)s;
  input->lens[i] = len;
  input->parts[i][len] = '\0';
  input->i++;
  return 1;
}

static void parse_input_line(char *line, char **dst, char **name) {
  struct input_line input = {{0}};

  /* Set default values */
  *dst = NULL;
  *name = NULL;

  /*
  example input:

  nonexistant
  127.0.0.1/24
  127.0.0.1 127.0.0.1
  example.com 93.184.216.34
  example.com 93.184.216.0/24
  example.com 2606:2800:220:1:248:1893:25c8:1946

  The first field can either be a range, subnet, address or a hostname that
  may or may not have a corresponding address. Hostnames are assumed to be
  resolved earlier and will not be resolved in this step.
  */
  str_map_fieldz(line, "\r\n\t ", mapfunc, &input);
  if (input.i == 1) {
    *dst = input.parts[0];
  } else if (input.i == 2) {
    *name = input.parts[0];
    *dst = input.parts[1];
  }
}

/* command line options */
struct opts {
  struct bgrab_opts bgrab;
  int no_sandbox;        /* 1 if the sandbox should be disabled */
  int tls;               /* 1 if TLS should be used */
  int tls_verify;        /* 1 if only conns with valid certs are OK */
  const char *inpath;
  const char *outpath;
  const char *ports;
  char *hostname;
};

static void print_bgrab_error(const char *err) {
  fprintf(stderr, "%s\n", err);
}

static void opts_or_die(struct opts *opts, int argc, char *argv[]) {
  int ch;
  const char *optstring = "n:t:Xc:d:i:o:z:sKH:p:";
  const char *argv0;
  struct option optcfgs[] = {
    {"max-clients",       required_argument, NULL, 'n'},
    {"timeout",           required_argument, NULL, 't'},
    {"no-sandbox",        no_argument,       NULL, 'X'},
    {"connects-per-tick", required_argument, NULL, 'c'},
    {"mdelay-per-tick",   required_argument, NULL, 'd'},
    {"in",                required_argument, NULL, 'i'},
    {"out",               required_argument, NULL, 'o'},
    {"tls",               no_argument,       NULL, 's'},
    {"tls-verify",        no_argument,       NULL, 'K'},
    {"hostname",          required_argument, NULL, 'H'},
    {"ports",             required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}
  };

  argv0 = argv[0];

  /* fill in defaults */
  opts->bgrab.max_clients = DFL_NCLIENTS;
  opts->bgrab.timeout = DFL_TIMEOUT;
  opts->bgrab.connects_per_tick = DFL_CONNECTS_PER_TICK;
  opts->bgrab.mdelay_per_tick = DFL_MDELAY_PER_TICK;
  opts->bgrab.on_error = print_bgrab_error;
  opts->inpath = DFL_INPATH;
  opts->outpath = DFL_OUTPATH;
  opts->tls = 0;
  opts->tls_verify = 0;
  opts->hostname = NULL;
  opts->ports = NULL;

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
    case 'i':
      opts->inpath = optarg;
      break;
    case 'o':
      opts->outpath = optarg;
      break;
    case 's':
      opts->tls = 1;
      break;
    case 'K':
      opts->tls_verify = 1;
    case 'H':
      opts->hostname = optarg;
      break;
    case 'p':
      opts->ports = optarg;
      break;
    default:
      goto usage;
    }
  }

  if (opts->ports == NULL) {
    fprintf(stderr, "missing port list\n");
    goto usage;
  }

  return;
usage:
  fprintf(stderr, "%s [opts]\n"
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
      "  -i|--in <path>\n"
      "      Input file (stdin)\n"
      "  -o|--out <path>\n"
      "      Output file (stdout)\n"
      "  -s|--tls\n"
      "      Use TLS\n"
      "  -K|--tls-verify\n"
      "      Only accept TLS connections with valid certificates\n"
      "  -H|--hostname <hostname>\n"
      "      Hostname to use, if any. Overrides any name given as input\n"
      "  -p|--ports <port-def>\n"
      "      Ports to use\n",
      argv0, DFL_NCLIENTS, DFL_TIMEOUT, DFL_CONNECTS_PER_TICK,
      DFL_MDELAY_PER_TICK);
  exit(EXIT_FAILURE);
}

int banners_main(struct scan_ctx *scan, int argc, char *argv[]) {
  int ret;
  struct opts opts = {0};
  struct bgrab_ctx grabber;
  struct tcpsrc_ctx tcpsrc;
  struct lines_ctx linebuf;
  int status = EXIT_FAILURE;
  FILE *outfile = NULL;
  FILE *infile = NULL;
  SSL_CTX *ssl_ctx = NULL;
  int chunkret;

  /* parse command line arguments to option struct */
  opts_or_die(&opts, argc, argv);

  /* if TLS is to be used, initiate the library */
  if (opts.tls) {
    SSL_library_init();
  }

  /* initialize the TCP client connection source */
  ret = tcpsrc_init(&tcpsrc);
  if (ret != 0) {
    perror("tcpsrc_init");
    goto done;
  }

  /* enter sandbox mode unless sandbox is disabled */
  if (opts.no_sandbox) {
    fprintf(stderr, "warning: sandbox disabled\n");
  } else {
    ret = sandbox_enter();
    if (ret != 0) {
      fprintf(stderr, "failed to enter sandbox mode\n");
      goto done_tcpsrc_cleanup;
    }
  }

  /* if TLS is to be used, initialize the TLS context. Done after
   * sandbox_enter. */
  if (opts.tls) {
    ssl_ctx = SSL_CTX_new(SSLv23_client_method());
    if (ssl_ctx == NULL) {
      fprintf(stderr, "failed to initialize the TLS context\n");
      goto done_tcpsrc_cleanup;
    }

    if (opts.tls_verify) {
      SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL); 
    } else {
      SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL); 
    }
  }

  ret = opener_fopen(&scan->opener, opts.outpath, "wb", &outfile);
  if (ret < 0) {
    fprintf(stderr, "%s: %s\n", opts.outpath,
        opener_strerror(&scan->opener));
    goto done_ssl_ctx_free;
  }

  ret = opener_fopen(&scan->opener, opts.inpath, "rb", &infile);
  if (ret < 0) {
    fprintf(stderr, "%s: %s\n", opts.inpath,
        opener_strerror(&scan->opener));
    goto done_fclose_outfile;
  }

  /* set-up the input line buffering */
  ret = lines_init(&linebuf, infile);
  if (ret != LINES_OK) {
    fprintf(stderr, "failed to set up input line buffer\n");
    goto done_fclose_infile;
  }

  /* initialize the banner grabber */
  opts.bgrab.outfile = outfile;
  opts.bgrab.ssl_ctx = ssl_ctx;
  if (bgrab_init(&grabber, &opts.bgrab, tcpsrc) < 0) {
    fprintf(stderr, "bgrab_init: %s\n", bgrab_strerror(&grabber));
    goto done_lines_cleanup;
  }

  /* Process the input in chunks */
  while((chunkret = lines_next_chunk(&linebuf)) == LINES_CONTINUE) {
    char *line;
    char *dst;
    char *name;
    /* load a set of destinations */
    while (lines_next(&linebuf, &line, NULL) == LINES_CONTINUE) {
      parse_input_line(line, &dst, &name);
      if (dst != NULL) {
        if (opts.hostname != NULL) {
          /* hostname override */
          name = *opts.hostname ? opts.hostname : NULL;
        }

        ret = bgrab_add_dsts(&grabber, dst, opts.ports, name);
        if (ret != 0) {
          /* this is OK - dst may be an unresolved host name. Log and move
           * on. */
          fprintf(stderr, "bgrab: warning: unable to grab dst %s\n", dst);
        }
      }
    }

    /* Grab all the banners! */
    ret = bgrab_run(&grabber);
    if (ret < 0) {
      fprintf(stderr, "bgrab_run: %s\n", bgrab_strerror(&grabber));
      goto done_bgrab_cleanup;
    }
  }

  if (LINES_IS_ERR(chunkret)) {
    fprintf(stderr, "failed to read input chunk: %s\n",
        lines_strerror(chunkret));
    goto done_bgrab_cleanup;
  }

  status = EXIT_SUCCESS;
done_bgrab_cleanup:
  bgrab_cleanup(&grabber);
done_lines_cleanup:
  lines_cleanup(&linebuf);
done_fclose_infile:
  fclose(infile);
done_fclose_outfile:
  fclose(outfile);
done_ssl_ctx_free:
  if (ssl_ctx != NULL) {
    SSL_CTX_free(ssl_ctx);
  }
done_tcpsrc_cleanup:
  tcpsrc_cleanup(&tcpsrc);
done:
  return status;
}
