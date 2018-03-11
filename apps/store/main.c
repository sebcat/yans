#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var"
#endif

#define DFL_STOREPATH LOCALSTATEDIR "/stored/stored.sock"

static char databuf_[32768]; /* get/put buffer */

static int run_get(const char *socket, const char *id, const char *filename) {
  int ret;
  int result = -1;
  int getfd = -1;
  struct ycl_ctx ctx;
  struct ycl_msg msg;
  struct ycl_msg_store_enter entermsg = {{0}};
  struct ycl_msg_status_resp respmsg = {{0}};
  struct ycl_msg_store_open openmsg = {{0}};

  ret = ycl_connect(&ctx, socket);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_connect: %s\n", ycl_strerror(&ctx));
    return -1;
  }

  ret = ycl_msg_init(&msg);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_msg_init failure\n");
    goto ycl_cleanup;
  }

  entermsg.store_id.data = id;
  entermsg.store_id.len = strlen(id);
  ret = ycl_msg_create_store_enter(&msg, &entermsg);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_msg_create_store_enter failure\n");
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
    fprintf(stderr, "failed to receive enter response: %s\n",
        ycl_strerror(&ctx));
    goto ycl_msg_cleanup;
  }

  ret = ycl_msg_parse_status_resp(&msg, &respmsg);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to parse enter response\n");
    goto ycl_msg_cleanup;
  }

  if (respmsg.errmsg.data != NULL && *respmsg.errmsg.data != '\0') {
    fprintf(stderr, "received failure: %s\n", respmsg.errmsg.data);
    goto ycl_msg_cleanup;
  }

  openmsg.path.data = filename;
  openmsg.path.len = strlen(filename);
  openmsg.flags = O_RDONLY;
  ret = ycl_msg_create_store_open(&msg, &openmsg);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to serialize open request\n");
    goto ycl_msg_cleanup;
  }

  ret = ycl_sendmsg(&ctx, &msg);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to send open request: %s\n", ycl_strerror(&ctx));
    goto ycl_msg_cleanup;
  }

  ret = ycl_recvfd(&ctx, &getfd);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to receive fd: %s\n", ycl_strerror(&ctx));
    goto ycl_msg_cleanup;
  }

  for (;;) {
    ssize_t nread;
    ssize_t nwritten;
    char *curr;
    size_t left;

    nread = read(getfd, databuf_, sizeof(databuf_));
    if (nread == 0) {
      break;
    } else if (nread < 0) {
      fprintf(stderr, "store read error: %s\n", strerror(errno));
      goto getfd_cleanup;
    }

    left = nread;
    curr = databuf_;
    while (left > 0) {
      nwritten = write(STDOUT_FILENO, curr, left);
      if (nwritten < 0) {
        fprintf(stderr, "write error: %s\n", strerror(errno));
        goto getfd_cleanup;
      } else if (nwritten == 0) {
        fprintf(stderr, "premature EOF\n");
        goto getfd_cleanup;
      }
      left -= nwritten;
      curr += nwritten;
    }
  }

  result = 0;
getfd_cleanup:
  close(getfd);
ycl_msg_cleanup:
  ycl_msg_cleanup(&msg);
ycl_cleanup:
  ycl_close(&ctx);
  return result;
}

static int run_put(const char *socket, const char *id, const char *filename) {
  int ret;
  int result = -1;
  int putfd = -1;
  struct ycl_ctx ctx;
  struct ycl_msg msg;
  struct ycl_msg_store_enter entermsg = {{0}};
  struct ycl_msg_status_resp respmsg = {{0}};
  struct ycl_msg_store_open openmsg = {{0}};

  ret = ycl_connect(&ctx, socket);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_connect: %s\n", ycl_strerror(&ctx));
    return -1;
  }

  ret = ycl_msg_init(&msg);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_msg_init failure\n");
    goto ycl_cleanup;
  }

  entermsg.store_id.data = id;
  entermsg.store_id.len = strlen(id);
  ret = ycl_msg_create_store_enter(&msg, &entermsg);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_msg_create_store_enter failure\n");
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
    fprintf(stderr, "failed to receive enter response: %s\n",
        ycl_strerror(&ctx));
    goto ycl_msg_cleanup;
  }

  ret = ycl_msg_parse_status_resp(&msg, &respmsg);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to parse enter response\n");
    goto ycl_msg_cleanup;
  }

  if (respmsg.errmsg.data != NULL && *respmsg.errmsg.data != '\0') {
    fprintf(stderr, "received failure: %s\n", respmsg.errmsg.data);
    goto ycl_msg_cleanup;
  }

  if (respmsg.okmsg.data != NULL && *respmsg.okmsg.data != '\0') {
    printf("%s\n", respmsg.okmsg.data);
  }

  openmsg.path.data = filename;
  openmsg.path.len = strlen(filename);
  openmsg.flags = O_WRONLY | O_CREAT | O_TRUNC;
  ret = ycl_msg_create_store_open(&msg, &openmsg);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to serialize open request\n");
    goto ycl_msg_cleanup;
  }

  ret = ycl_sendmsg(&ctx, &msg);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to send open request: %s\n", ycl_strerror(&ctx));
    goto ycl_msg_cleanup;
  }

  ret = ycl_recvfd(&ctx, &putfd);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to receive fd: %s\n", ycl_strerror(&ctx));
    goto ycl_msg_cleanup;
  }

  for (;;) {
    ssize_t nread;
    ssize_t nwritten;
    char *curr;
    size_t left;

    nread = read(STDIN_FILENO, databuf_, sizeof(databuf_));
    if (nread == 0) {
      break;
    } else if (nread < 0) {
      fprintf(stderr, "stdin read error: %s\n", strerror(errno));
      goto putfd_cleanup;
    }

    left = nread;
    curr = databuf_;
    while (left > 0) {
      nwritten = write(putfd, curr, left);
      if (nwritten < 0) {
        fprintf(stderr, "write error: %s\n", strerror(errno));
        goto putfd_cleanup;
      } else if (nwritten == 0) {
        fprintf(stderr, "premature EOF\n");
        goto putfd_cleanup;
      }
      left -= nwritten;
      curr += nwritten;
    }
  }

  result = 0;
putfd_cleanup:
  close(putfd);
ycl_msg_cleanup:
  ycl_msg_cleanup(&msg);
ycl_cleanup:
  ycl_close(&ctx);
  return result;
}

static int put_main(int argc, char *argv[]) {
  int ch;
  int ret;
  const char *optstr = "hs:i:";
  const char *socket = DFL_STOREPATH;
  const char *id = NULL;
  const char *filename = NULL;
  struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"socket", required_argument, NULL, 's'},
    {"id", required_argument, NULL, 'i'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc-1, argv+1, optstr, longopts, NULL)) != -1) {
    switch(ch) {
    case 's':
      socket = optarg;
      break;
    case 'i':
      id = optarg;
      break;
    case 'h':
    default:
      goto usage;
    }
  }

  if (optind + 1 >= argc) {
    fprintf(stderr, "missing file name\n");
    goto usage;
  }

  filename = argv[optind + 1];
  ret = run_put(socket, id, filename);
  if (ret < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
usage:
  fprintf(stderr, "usage: %s put [opts] <name>\n"
      "opts:\n"
      "  -s|--socket   store socket path (%s)\n"
      "  -i|--id       store ID\n",
      argv[0], DFL_STOREPATH);
  return EXIT_FAILURE;
}

static int get_main(int argc, char *argv[]) {
  int ch;
  int ret;
  const char *optstr = "hs:";
  const char *socket = DFL_STOREPATH;
  const char *id = NULL;
  const char *filename = NULL;
  struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"socket", required_argument, NULL, 's'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc-1, argv+1, optstr, longopts, NULL)) != -1) {
    switch(ch) {
    case 's':
      socket = optarg;
      break;
    case 'h':
    default:
      goto usage;
    }
  }

  if (optind + 1 >= argc) {
    fprintf(stderr, "missing store ID\n");
    goto usage;
  }
  id = argv[optind + 1];

  if (optind + 2 >= argc) {
    fprintf(stderr, "missing file name\n");
    goto usage;
  }
  filename = argv[optind + 2];

  ret = run_get(socket, id, filename);
  if (ret < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
usage:
  fprintf(stderr, "usage: %s get [opts] <id> <name>\n"
      "opts:\n"
      "  -s|--socket   store socket path (%s)\n",
      argv[0], DFL_STOREPATH);
  return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    goto usage;
  }

  if (strcmp(argv[1], "put") == 0) {
    return put_main(argc, argv);
  } else if (strcmp(argv[1], "get") == 0) {
    return get_main(argc, argv);
  }

  fprintf(stderr, "unknown command\n");
usage:
  fprintf(stderr, "usage: %s <command> [args]\n"
      "commands:\n"
      "  put - put a file in the store\n"
      "  get - get file content from a store\n",
      argv[0]);
  return EXIT_FAILURE;
}
