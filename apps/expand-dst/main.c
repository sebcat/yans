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
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include <lib/net/dsts.h>

#define DFL_NTIMES 1

struct opts {
  int ntimes;    /* number of repetitions */
  int npairs;    /* number of addrs, ports pairs*/
  char **pairs;  /* addrs ports pairs */
};

static void opts_or_die(struct opts *opts, int argc, char *argv[]) {
  int ch;
  const char *optstring = "n:h:";
  const char *argv0;
  struct option optcfgs[] = {
    {"help",     required_argument, NULL, 'h'},
    {"ntimes",   required_argument, NULL, 'n'},
    {NULL, 0, NULL, 0}
  };

  opts->ntimes = DFL_NTIMES;

  argv0 = argv[0];
  while ((ch = getopt_long(argc, argv, optstring, optcfgs, NULL)) != -1) {
    switch(ch) {
    case 'n':
      opts->ntimes = (int)strtol(optarg, NULL, 10);
      break;
    case 'h':
    default:
      goto usage;
    }
  }

  argc -= optind;
  argv += optind;
  if (argc < 2) {
    fprintf(stderr, "Missing addrs ports\n");
    goto usage;
  } else if (argc & 1) {
    fprintf(stderr, "odd number of positional arguments\n");
    goto usage;
  }

  opts->npairs = argc / 2;
  opts->pairs = argv;

  return;
usage:
  fprintf(stderr, "usage: %s [opts] <addrs0> <ports0> .. [addrsN] [portsN]\n"
      "opts:\n"
      "  -h|--help            this message\n"
      "  -n|--ntimes <n>      number of repetitions in output (%d)\n"
      , argv0, DFL_NTIMES);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  static struct dsts_ctx dsts;
  static struct opts opts;
  static char hostbuf[256];
  static char portbuf[24];

  int status = EXIT_FAILURE;
  int i;
  int ret;
  socklen_t saddrlen = 0;
  union {
    struct sockaddr sa;
    struct sockaddr_in in;
    struct sockaddr_in6 in6;
  } saddr;

  opts_or_die(&opts, argc, argv);

  ret = dsts_init(&dsts);
  if (ret < 0) {
    fprintf(stderr, "dsts_init failure\n");
    goto out;
  }

  for (i = 0; i < opts.npairs; i++) {
    dsts_add(&dsts, opts.pairs[i*2], opts.pairs[i*2+1], NULL);
  }

  for (i = 0; i < opts.ntimes; i++) {
    while (dsts_next(&dsts, &saddr.sa, &saddrlen, NULL)) {
      ret = getnameinfo(&saddr.sa, saddrlen, hostbuf, sizeof(hostbuf),
          portbuf, sizeof(portbuf), NI_NUMERICHOST | NI_NUMERICSERV);
      if (ret != 0) {
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(ret));
        continue;
      }

      printf("%s %s\n", hostbuf, portbuf);
    }
  }

  status = EXIT_SUCCESS;
out:
  dsts_cleanup(&dsts);
  return status;
}
