#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>

#include <lib/util/buf.h>
#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var"
#endif

#define DFL_KNEGDSOCK LOCALSTATEDIR "/knegd/knegd.sock"
#define TARGET_BUFSZ 1024 /* initial target buffer size */

struct subcmd {
  const char *name;
  int (*func)(int, char **);
};

struct start_opts {
  const char *sock;
  const char *type;
  long timeout;
  char **params;
  size_t nparams;
};

static int reqresp(const char *path, struct ycl_msg_knegd_req *req) {
  struct ycl_ctx ctx = {0};
  struct ycl_msg_status_resp resp = {0};
  struct ycl_msg msg = {{0}};
  int ret;
  int res = -1;

  ret = ycl_connect(&ctx, path);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_connect: %s\n", ycl_strerror(&ctx));
    return -1;
  }

  ret = ycl_msg_init(&msg);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to initialize ycl_msg\n");
    goto ycl_cleanup;
  }

  ret = ycl_msg_create_knegd_req(&msg, req);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to create knegd request\n");
    goto ycl_msg_cleanup;
  }

  ret = ycl_sendmsg(&ctx, &msg);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_sendmsg: %s\n", ycl_strerror(&ctx));
    goto ycl_msg_cleanup;

  }

  ycl_msg_reset(&msg);
  ret = ycl_recvmsg(&ctx, &msg);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_recvmsg: %s\n", ycl_strerror(&ctx));
    goto ycl_msg_cleanup;
  }

  ret = ycl_msg_parse_status_resp(&msg, &resp);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to parse status response\n");
    goto ycl_msg_cleanup;
  }

  if (resp.errmsg != NULL) {
    fprintf(stderr, "received error: %s\n", resp.errmsg);
    goto ycl_msg_cleanup;
  }

  if (resp.okmsg != NULL) {
    printf("%s\n", resp.okmsg);
  }

  res = 0; /* signal success */
ycl_msg_cleanup:
  ycl_msg_cleanup(&msg);

ycl_cleanup:
  ycl_close(&ctx);
  return res;
}

static int pid(int argc, char **argv) {
  int ret;
  int ch;
  struct ycl_msg_knegd_req req = {0};
  const char *optstr = "hs:";
  const char *argv0 = argv[0];
  const char *sock = DFL_KNEGDSOCK;
  static struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"socket", required_argument, NULL, 's'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc-1, argv+1, optstr, longopts, NULL)) != -1) {
    switch(ch) {
    case 's':
      sock = optarg;
      break;
    case 'h':
    case '?':
      goto usage;
    }
  }

  argc -= optind + 1;
  argv += optind + 1;
  if (argc <= 0) {
    fprintf(stderr, "missing id\n");
    goto usage;
  }

  req.action = "pid";
  req.id = argv[0];
  ret = reqresp(sock, &req);
  if (ret < 0) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;

usage:
  fprintf(stderr, "usage: %s pid [opts] <id>\n"
      "opts:\n"
      "  -h|--help           - this text\n"
      "  -s|--socket <path>  - path to knegd socket (%s)\n"
      , argv0, DFL_KNEGDSOCK);
  return EXIT_FAILURE;
}

static int stop(int argc, char **argv) {
  int ret;
  int ch;
  struct ycl_msg_knegd_req req = {0};
  const char *optstr = "hs:";
  const char *argv0 = argv[0];
  const char *sock = DFL_KNEGDSOCK;
  static struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"socket", required_argument, NULL, 's'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc-1, argv+1, optstr, longopts, NULL)) != -1) {
    switch(ch) {
    case 's':
      sock = optarg;
      break;
    case 'h':
    case '?':
      goto usage;
    }
  }

  argc -= optind + 1;
  argv += optind + 1;
  if (argc <= 0) {
    fprintf(stderr, "missing id\n");
    goto usage;
  }

  req.action = "stop";
  req.id = argv[0];
  ret = reqresp(sock, &req);
  if (ret < 0) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;

usage:
  fprintf(stderr, "usage: %s stop [opts] <id>\n"
      "opts:\n"
      "  -h|--help           - this text\n"
      "  -s|--socket <path>  - path to knegd socket (%s)\n"
      , argv0, DFL_KNEGDSOCK);
  return EXIT_FAILURE;
}

static int run_start(struct start_opts *opts) {
  struct ycl_msg_knegd_req req = {0};
  req.action = "start";
  req.timeout = opts->timeout;
  req.type = opts->type;
  req.params = (const char**)opts->params;
  req.nparams = opts->nparams;
  return reqresp(opts->sock, &req);
}

static int start(int argc, char **argv) {
  int ch;
  int ret;
  long l;
  const char *optstr = "hp:s:t:";
  char **params = NULL;
  char **tmp;
  size_t nparams = 0;
  static struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"param", required_argument, NULL, 'p'},
    {"socket", required_argument, NULL, 's'},
    {"timeout", required_argument, NULL, 't'},
    {NULL, 0, NULL, 0},
  };
  struct start_opts opts = {
    .sock = DFL_KNEGDSOCK,
    .timeout = 0,
    .type = NULL,
  };

  while ((ch = getopt_long(argc-1, argv+1, optstr, longopts, NULL)) != -1) {
    switch(ch) {
    case 's':
      opts.sock = optarg;
      break;
    case 't':
      l = strtol(optarg, NULL, 10);
      if (l <= 0 || l == LONG_MAX) {
        fprintf(stderr, "invalid timeout\n");
        exit(EXIT_FAILURE);
      }
      opts.timeout = l;
      break;
    case 'p':
      tmp = realloc(params, sizeof(char *) * (nparams + 1));
      if (tmp == NULL) {
        fprintf(stderr, "%s\n", strerror(errno));
        if (params != NULL) {
          free(params);
        }
        exit(EXIT_FAILURE);
      }
      params = tmp;
      params[nparams++] = optarg;
      break;
    case 'h':
    case '?':
      goto usage;
    }
  }

  argc -= optind + 1;
  argv += optind + 1;
  if (argc > 0) {
    opts.type = argv[0];
  }

  if (opts.type == NULL) {
    fprintf(stderr, "kneg type missing\n");
    goto fail;
  }

  opts.params = params;
  opts.nparams = nparams;
  ret = run_start(&opts);
  if (params != NULL) {
    free(params);
  }
  if (ret < 0) {
    goto fail;
  }

  return EXIT_SUCCESS;

usage:
  fprintf(stderr, "usage: [opts] <type>\n"
      "opts:\n"
      "  -h|--help           - this text\n"
      "  -s|--socket <path>  - path to knegd socket (%s)\n"
      "  -t|--type   <type>  - kneg type\n"
      "  -p|--param  <param> - kneg parameter\n",
      DFL_KNEGDSOCK);
fail:
  return EXIT_FAILURE;
}

static void usage(const char *argv0, const struct subcmd *cmds) {
  size_t i;

  fprintf(stderr, "usage:\n"
      "  %s <subcmd> [opts]\n"
      "subcmds:\n", argv0);
  for (i = 0; cmds[i].name != NULL; i++) {
    fprintf(stderr, "  %s\n", cmds[i].name);
  }

  exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
  int i;
  static const struct subcmd subcmds[] = {
    {"start", start},
    {"pid", pid},
    {"stop", stop},
    {NULL, NULL},
  };

  if (argc < 2 || strcmp(argv[1], "-h") == 0 ||
      strcmp(argv[1], "--help") == 0) {
    usage(argv[0] == NULL ? "kneg" : argv[0], subcmds);
  }

  for (i = 0; subcmds[i].name != NULL; i++) {
    if (strcmp(argv[1], subcmds[i].name) == 0) {
      return subcmds[i].func(argc, argv);
    }
  }

  usage(argv[0], subcmds);
  return EXIT_FAILURE;
}