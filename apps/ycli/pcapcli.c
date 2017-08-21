#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>

#include <lib/ycl/ycl.h>

#include <apps/ycli/pcapcli.h>

#define DEFAULT_SOCK "/var/ethd/pcap.sock"

struct pcapcli_opts {
  char *iface;
  char *filter;
  char *output;
  char *sock;
};

static int pcapcli_opts(struct pcapcli_opts *res, int argc, char *argv[]) {
  int ch;
  static const struct option opts[] = {
    {"iface", required_argument, NULL, 'i'},
    {"filter", required_argument, NULL, 'f'},
    {"output", required_argument, NULL, 'o'},
    {"socket", required_argument, NULL, 's'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc, argv, "i:f:o:s:h", opts, NULL)) != -1) {
    switch(ch) {
    case 'i':
      res->iface = optarg;
      break;
    case 'f':
      res->filter = optarg;
      break;
    case 'o':
      res->output = optarg;
      break;
    case 's':
      res->sock = optarg;
      break;
    case 'h':
    default:
      goto usage;
    }
  }

  if (res->iface == NULL) {
    fprintf(stderr, "no iface specified\n");
    goto usage;
  }

  if (res->output == NULL) {
    fprintf(stderr, "no output specified\n");
    goto usage;
  }

  if (res->sock == NULL) {
    res->sock = DEFAULT_SOCK;
  }

  return 0;

usage:
  fprintf(stderr, "usage: pcap --iface|-i <iface> --filter|-f <filter> "
      "--output|-o <path>\n");
  return -1;
}

static volatile sig_atomic_t got_shutdown_sig = 0;

static void handle_shutdown_sig(int sig) {
  if (sig == SIGINT || sig == SIGTERM || sig == SIGHUP) {
    got_shutdown_sig = 1;
  }
}

static int pcapcli_wait(struct ycl_ctx *ycl, struct ycl_msg *msg) {
  struct sigaction sa = {{0}};
  struct pollfd p;
  int ret;

  /* setup the signal handler for termination */
  sa.sa_handler = handle_shutdown_sig;
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  p.fd = ycl_fd(ycl);
  p.events = POLLIN;
  do {
    if (got_shutdown_sig) {
      printf("Received shutdown signal, stopping capture...\n");
      if (ycl_msg_create_pcap_close(msg) != YCL_OK) {
        fprintf(stderr, "unable to create pcap close message\n");
        goto fail;
      }

      if (ycl_sendmsg(ycl, msg) != YCL_OK) {
        fprintf(stderr, "unable to send pcap close message: %s\n",
            ycl_strerror(ycl));
        goto fail;
      }
    }
    ret = poll(&p, 1, -1);
  } while (ret < 0 && errno == EINTR);

  if (ret < 0) {
    fprintf(stderr, "poll: %s\n", strerror(errno));
    goto fail;
  }

  return 0;

fail:
  return -1;
}

static int pcapcli_run(int fd, struct pcapcli_opts *opts) {
  struct ycl_ctx ycl;
  struct ycl_msg msg = {{0}};
  const char *okmsg = NULL;
  const char *errmsg = NULL;

  if (ycl_connect(&ycl, opts->sock) != YCL_OK) {
    fprintf(stderr, "%s\n", ycl_strerror(&ycl));
    return -1;
  }

  ycl_msg_init(&msg);
  if (ycl_sendfd(&ycl, fd) != YCL_OK) {
    fprintf(stderr, "%s\n", ycl_strerror(&ycl));
    goto fail;
  }

  if (ycl_msg_create_pcap_req(&msg, opts->iface, opts->filter) != YCL_OK) {
    fprintf(stderr, "unable to create pcap request\n");
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
    fprintf(stderr, "unable to parse pcap status response\n");
    goto fail;
  }

  if (errmsg != NULL) {
    fprintf(stderr, "%s\n", errmsg);
    goto fail;
  }

  if (pcapcli_wait(&ycl, &msg) < 0) {
    goto fail;
  }

  ycl_msg_cleanup(&msg);
  ycl_close(&ycl);
  return 0;
fail:
  ycl_msg_cleanup(&msg);
  ycl_close(&ycl);
  return -1;
}

int pcapcli_main(int argc, char *argv[]) {
  struct pcapcli_opts opts = {0};
  int ofd;
  int ret;

  if (pcapcli_opts(&opts, argc, argv) < 0) {
    return EXIT_FAILURE;
  }

  ofd = open(opts.output, O_WRONLY|O_CREAT, 0755);
  if (ofd < 0) {
    fprintf(stderr, "output file error: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  ret = pcapcli_run(ofd, &opts);
  close(ofd);
  return (ret < 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}

