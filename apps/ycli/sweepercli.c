#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include <lib/ycl/ycl.h>

#define DEFAULT_SOCK "/var/ethd/sweeper.sock"

struct sweepercli_opts {
  int arp;
  char *addrs;
  char *sock;
};

static int parse_opts(struct sweepercli_opts *opts, int argc, char *argv[]) {
  int ch;
  static const struct option ps[] = {
    {"sock", required_argument, NULL, 's'},
    {"arp", no_argument, NULL, 'a'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
  };

  opts->sock = DEFAULT_SOCK;

  while ((ch = getopt_long(argc, argv, "s:ah", ps, NULL)) != -1) {
    switch(ch) {
      case 's':
        opts->sock = optarg;
        break;
      case 'a':
        opts->arp = 1;
        break;
      case 'h':
      default:
        goto usage;
    }
  }

  argc -= optind;
  argv += optind;
  if (argc > 0) {
    opts->addrs = argv[0];
  }

  if (opts->addrs == NULL) {
    goto usage;
  }

  return 0;

usage:
  fprintf(stderr, "usage: [-s|--sock <path>] [-a|--arp] <addrspec>\n");
  return -1;
}

static int sweepercli_run(struct sweepercli_opts *opts) {
  struct ycl_ctx ycl;
  struct ycl_msg msg = {0};
  const char *okmsg = NULL;
  const char *errmsg = NULL;
  struct ycl_msg_sweeper_req req;

  if (ycl_connect(&ycl, opts->sock) != YCL_OK) {
    fprintf(stderr, "%s\n", ycl_strerror(&ycl));
    return -1;
  }

  req.arp = opts->arp ? "yes" : "no";
  req.addrs = opts->addrs;
  ycl_msg_init(&msg);
  if (ycl_msg_create_sweeper_req(&msg, &req) != YCL_OK) {
    fprintf(stderr, "unable to create sweeper request\n");
    goto fail;
  }

  if (ycl_sendmsg(&ycl, &msg) < 0) {
    fprintf(stderr, "%s\n", ycl_strerror(&ycl));
    goto fail;
  }

  ycl_msg_reset(&msg);
  if (ycl_recvmsg(&ycl, &msg) != YCL_OK) {
    fprintf(stderr, "%s\n", ycl_strerror(&ycl));
    goto fail;
  }

  if (ycl_msg_parse_status_resp(&msg, &okmsg, &errmsg) != YCL_OK) {
    fprintf(stderr, "unable to parse sweeper status response\n");
    goto fail;
  }

  if (errmsg != NULL) {
    fprintf(stderr, "%s\n", errmsg);
    goto fail;
  } else if (okmsg != NULL) {
    fprintf(stderr, "%s\n", okmsg);
  }

  ycl_msg_cleanup(&msg);
  ycl_close(&ycl);
  return 0;
fail:
  ycl_msg_cleanup(&msg);
  ycl_close(&ycl);
  return -1;
}

int sweepercli_main(int argc, char *argv[]) {
  struct sweepercli_opts opts = {0};
  if (parse_opts(&opts, argc, argv) < 0) {
    return EXIT_FAILURE;
  }

  if (sweepercli_run(&opts) < 0) {
    goto fail;
  }

  return EXIT_SUCCESS;
fail:
  return EXIT_FAILURE;
}
