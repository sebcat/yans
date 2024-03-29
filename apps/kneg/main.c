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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>

#include <lib/util/buf.h>
#include <lib/util/sandbox.h>
#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var"
#endif

#define DFL_KNEGDSOCK LOCALSTATEDIR "/yans/knegd/knegd.sock"
#define TARGET_BUFSZ 1024 /* initial target buffer size */

struct subcmd {
  const char *name;
  int (*func)(int, char **);
};

struct start_opts {
  const char *sock;
  const char *type;
  const char *id;
  const char *name;
  long timeout;
};

static int reqresp(const char *path, struct ycl_msg_knegd_req *req) {
  struct ycl_ctx ctx = {0};
  struct ycl_msg_status_resp resp = {{0}};
  struct ycl_msg msg = {{0}};
  int ret;
  int res = -1;

  ret = ycl_connect(&ctx, path);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_connect: %s\n", ycl_strerror(&ctx));
    return -1;
  }

  ret = sandbox_enter();
  if (ret < 0) {
    fprintf(stderr, "sandbox_enter failure\n");
    goto ycl_cleanup;
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

  if (resp.errmsg.data != NULL) {
    fprintf(stderr, "received error: %s\n", resp.errmsg.data);
    goto ycl_msg_cleanup;
  }

  if (resp.okmsg.data != NULL) {
    printf("%s\n", resp.okmsg.data);
  }

  res = 0; /* signal success */
ycl_msg_cleanup:
  ycl_msg_cleanup(&msg);

ycl_cleanup:
  ycl_close(&ctx);
  return res;
}

static int queue(int argc, char **argv) {
  long l;
  int ret;
  int ch;
  struct ycl_msg_knegd_req req = {0};
  const char *optstr = "hs:t:n:";
  const char *argv0 = argv[0];
  const char *sock = DFL_KNEGDSOCK;
  static struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"socket", required_argument, NULL, 's'},
    {"timeout", required_argument, NULL, 't'},
    {"name", required_argument, NULL, 'n'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc-1, argv+1, optstr, longopts, NULL)) != -1) {
    switch(ch) {
    case 's':
      sock = optarg;
      break;
    case 't':
      l = strtol(optarg, NULL, 10);
      if (l <= 0 || l == LONG_MAX) {
        fprintf(stderr, "invalid timeout\n");
        exit(EXIT_FAILURE);
      }
      req.timeout = l;
      break;
    case 'n':
      req.name.data = optarg;
      req.name.len = strlen(optarg);
      break;
    case 'h':
    case '?':
      goto usage;
    }
  }

  argc -= optind + 1;
  argv += optind + 1;
  if (argc != 2) {
    goto usage;
  }

  req.action.data = "queue";
  req.action.len = sizeof("queue")-1;
  req.id.data = argv[0];
  req.id.len = strlen(argv[0]);
  req.type.data = argv[1];
  req.type.len = strlen(argv[1]);
  ret = reqresp(sock, &req);
  if (ret < 0) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;

usage:
  fprintf(stderr, "usage: %s queue [opts] <id> <type>\n"
      "opts:\n"
      "  -h|--help            - this text\n"
      "  -s|--socket  <path>  - path to knegd socket (%s)\n"
      "  -t|--timeout <n>     - timeout, in seconds\n"
      "  -n|--name    <name>  - name of job\n"
      , argv0, DFL_KNEGDSOCK);
  return EXIT_FAILURE;
}

static int manifest(int argc, char **argv) {
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

  req.action.data = "manifest";
  req.action.len = sizeof("manifest")-1;
  ret = reqresp(sock, &req);
  if (ret < 0) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;

usage:
  fprintf(stderr, "usage: %s manifest [opts]\n"
      "opts:\n"
      "  -h|--help           - this text\n"
      "  -s|--socket <path>  - path to knegd socket (%s)\n"
      , argv0, DFL_KNEGDSOCK);
  return EXIT_FAILURE;
}

static int queueinfo(int argc, char **argv) {
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

  req.action.data = "queueinfo";
  req.action.len = sizeof("queueinfo")-1;
  ret = reqresp(sock, &req);
  if (ret < 0) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;

usage:
  fprintf(stderr, "usage: %s queueinfo [opts]\n"
      "opts:\n"
      "  -h|--help           - this text\n"
      "  -s|--socket <path>  - path to knegd socket (%s)\n"
      , argv0, DFL_KNEGDSOCK);
  return EXIT_FAILURE;
}
static int pids(int argc, char **argv) {
  int ret;
  int ch;
  int i;
  buf_t b;
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
  buf_init(&b, 1024);
  for (i = 0; i < argc; i++) {
    buf_adata(&b, argv[i], strlen(argv[i]) + 1);
  }

  req.action.data = "pids";
  req.action.len = sizeof("pids")-1;
  req.id.data = b.data;
  req.id.len = b.len;
  ret = reqresp(sock, &req);
  buf_cleanup(&b);
  if (ret < 0) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;

usage:
  fprintf(stderr, "usage: %s pids [opts] <id0> [id1] ... [idN]\n"
      "opts:\n"
      "  -h|--help           - this text\n"
      "  -s|--socket <path>  - path to knegd socket (%s)\n"
      , argv0, DFL_KNEGDSOCK);
  return EXIT_FAILURE;
}

static int status(int argc, char **argv) {
  int ret;
  int ch;
  int i;
  buf_t b;
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
  buf_init(&b, 1024);
  for (i = 0; i < argc; i++) {
    buf_adata(&b, argv[i], strlen(argv[i]) + 1);
  }

  req.action.data = "status";
  req.action.len = sizeof("status")-1;
  req.id.data = b.data;
  req.id.len = b.len;
  ret = reqresp(sock, &req);
  buf_cleanup(&b);
  if (ret < 0) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;

usage:
  fprintf(stderr, "usage: %s status [opts] <id0> [id1] ... [idN]\n"
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

  req.action.data = "stop";
  req.action.len = sizeof("stop")-1;
  req.id.data = argv[0];
  req.id.len = strlen(argv[0]);
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
  req.action.data = "start";
  req.action.len = sizeof("start")-1;
  req.timeout = opts->timeout;
  req.type.data = opts->type;
  req.type.len = opts->type != NULL ? strlen(opts->type) : 0;
  req.id.data = opts->id;
  req.id.len = opts->id != NULL ? strlen(opts->id) : 0;
  req.name.data = opts->name;
  req.name.len = opts->name != NULL ? strlen(opts->name) : 0;
  return reqresp(opts->sock, &req);
}

static int start(int argc, char **argv) {
  int ch;
  int ret;
  long l;
  const char *optstr = "hs:t:i:n:";
  static struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"socket", required_argument, NULL, 's'},
    {"timeout", required_argument, NULL, 't'},
    {"id", required_argument, NULL, 'i'},
    {"name", required_argument, NULL, 'n'},
    {NULL, 0, NULL, 0},
  };
  struct start_opts opts = {
    .sock    = DFL_KNEGDSOCK,
    .timeout = 0,
    .type    = NULL,
    .id      = NULL,
    .name    = NULL,
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
    case 'i':
      opts.id = optarg;
      break;
    case 'n':
      opts.name = optarg;
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

  ret = run_start(&opts);
  if (ret < 0) {
    goto fail;
  }

  return EXIT_SUCCESS;

usage:
  fprintf(stderr, "usage: [opts] <type>\n"
      "opts:\n"
      "  -h|--help            - this text\n"
      "  -i|--id              - store ID\n"
      "  -s|--socket  <path>  - path to knegd socket (%s)\n"
      "  -t|--timeout <n>     - timeout, in seconds\n"
      "  -n|--name    <name>  - name of job\n",
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
    {"manifest", manifest},
    {"queueinfo", queueinfo},
    {"pids", pids},
    {"status", status},
    {"queue", queue},
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
