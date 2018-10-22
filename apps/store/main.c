#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <lib/util/sandbox.h>
#include <lib/util/sindex.h>
#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var"
#endif

#define DFL_STOREPATH LOCALSTATEDIR "/yans/stored/stored.sock"
#define DFL_INDEX_NELEMS 25

static char databuf_[32768]; /* get/put buffer */

static int setup_ycl_state(struct ycl_ctx *ctx, const char *socket,
    struct ycl_msg *msg) {
  int ret;

  ret = ycl_connect(ctx, socket);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_connect: %s\n", ycl_strerror(ctx));
    goto fail;
  }

  ret = sandbox_enter();
  if (ret < 0) {
    fprintf(stderr, "sandbox_enter failure\n");
    goto ycl_cleanup;
  }

  ret = ycl_msg_init(msg);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_msg_init failure\n");
    goto ycl_cleanup;
  }

  return 0;
ycl_cleanup:
  ycl_close(ctx);
fail:
  return -1;
}

static int run_get(const char *socket, const char *id, const char *filename) {
  int ret;
  int result = -1;
  int getfd = -1;
  struct ycl_ctx ctx;
  struct ycl_msg msg;
  struct ycl_msg_store_req reqmsg = {{0}};
  struct ycl_msg_status_resp respmsg = {{0}};
  struct ycl_msg_store_entered_req openmsg = {{0}};

  ret = setup_ycl_state(&ctx, socket, &msg);
  if (ret < 0) {
    return -1;
  }

  reqmsg.action.data = "enter";
  reqmsg.action.len = sizeof("enter") - 1;
  reqmsg.store_id.data = id;
  reqmsg.store_id.len = strlen(id);
  ret = ycl_msg_create_store_req(&msg, &reqmsg);
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

  openmsg.action.data = "open";
  openmsg.action.len = 5;
  openmsg.open_path.data = filename;
  openmsg.open_path.len = strlen(filename);
  openmsg.open_flags = O_RDONLY;
  ret = ycl_msg_create_store_entered_req(&msg, &openmsg);
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

read_again:
    nread = read(getfd, databuf_, sizeof(databuf_));
    if (nread == 0) {
      break;
    } else if (nread < 0) {
      if (errno == EINTR) {
        goto read_again;
      } else {
        perror("read");
        goto getfd_cleanup;
      }
    }

    left = nread;
    curr = databuf_;
    while (left > 0) {
write_again:
      nwritten = write(STDOUT_FILENO, curr, left);
      if (nwritten < 0) {
        if (errno == EINTR) {
          goto write_again;
        } else {
          perror("write");
          goto getfd_cleanup;
        }
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
  ycl_close(&ctx);
  return result;
}

static int run_put(const char *socket, const char *id, const char *name,
    const char *filename, int flags) {
  int ret;
  int result = -1;
  int putfd = -1;
  struct ycl_ctx ctx;
  struct ycl_msg msg;
  struct ycl_msg_store_req reqmsg = {{0}};
  struct ycl_msg_status_resp respmsg = {{0}};
  struct ycl_msg_store_entered_req openmsg = {{0}};
  static time_t *no_see;

  ret = setup_ycl_state(&ctx, socket, &msg);
  if (ret < 0) {
    return -1;
  }

  reqmsg.action.data = "enter";
  reqmsg.action.len = sizeof("enter") - 1;
  reqmsg.store_id.data = id;
  reqmsg.store_id.len = id ? strlen(id) : 0;
  reqmsg.name.data = name;
  reqmsg.name.len = name ? strlen(name) : 0;
  reqmsg.indexed = (long)time(no_see);
  ret = ycl_msg_create_store_req(&msg, &reqmsg);
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

  openmsg.action.data = "open";
  openmsg.action.len = 5;
  openmsg.open_path.data = filename;
  openmsg.open_path.len = strlen(filename);
  openmsg.open_flags = flags;
  ret = ycl_msg_create_store_entered_req(&msg, &openmsg);
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

read_again:
    nread = read(STDIN_FILENO, databuf_, sizeof(databuf_));
    if (nread == 0) {
      break;
    } else if (nread < 0) {
      if (errno == EINTR) {
        goto read_again;
      } else {
        perror("read");
        goto putfd_cleanup;
      }
    }

    left = nread;
    curr = databuf_;
    while (left > 0) {
write_again:
      nwritten = write(putfd, curr, left);
      if (nwritten < 0) {
        if (errno == EINTR) {
          goto write_again;
        } else {
          perror("write");
          goto putfd_cleanup;
        }
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
  ycl_close(&ctx);
  return result;
}

static void print_list_entries(FILE *fp, const char *data, size_t len) {
  size_t pos;
  const char *str;
  const char *curr;

  for (str = data, pos = 0; pos < len; pos++) {
    curr = data + pos;
    if (*curr == '\0') {
      if (str < curr) {
        fprintf(fp, "%s\n", str);
      }
      str = curr + 1;
    }
  }
}

static void print_list_pairs(FILE *fp, const char *data, size_t len) {
  size_t pos;
  const char *namestr;
  const char *sizestr;
  char ch;
  enum {
    LP_IN_NAME,
    LP_IN_SIZE,
  } S = LP_IN_NAME;

  for (namestr = data, pos = 0; pos < len; pos++) {
    ch = data[pos];
    switch(S) {
    case LP_IN_NAME:
      if (ch == '\0') {
        sizestr = data + pos + 1;
        S = LP_IN_SIZE;
      }
      break;
    case LP_IN_SIZE:
      if (ch == '\0') {
        fprintf(fp, "%s %s\n", sizestr, namestr);
        namestr = data + pos + 1;
        S = LP_IN_NAME;
      }
      break;
    default:
      /* reset */
      S = LP_IN_NAME;
      namestr = data + pos;
    }
  }
}

static int run_list(const char *socket, const char *id,
      const char *must_match) {
  int result = -1;
  int ret;
  struct ycl_ctx ctx;
  struct ycl_msg msg;
  struct ycl_msg_store_req reqmsg = {{0}};
  struct ycl_msg_store_list respmsg = {{0}};

  ret = setup_ycl_state(&ctx, socket, &msg);
  if (ret < 0) {
    return -1;
  }

  reqmsg.action.data = "list";
  reqmsg.action.len = sizeof("list") - 1;
  reqmsg.store_id.data = id;
  reqmsg.store_id.len = id ? strlen(id) : 0;
  reqmsg.list_must_match.data = must_match;
  reqmsg.list_must_match.len = must_match ? strlen(must_match) : 0;
  ret = ycl_msg_create_store_req(&msg, &reqmsg);
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

  ret = ycl_msg_parse_store_list(&msg, &respmsg);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to parse store list response\n");
    goto ycl_msg_cleanup;
  }

  if (respmsg.errmsg.len > 0) {
    fprintf(stderr, "%s\n", respmsg.errmsg.data);
    goto ycl_msg_cleanup;
  }

  if (reqmsg.store_id.len) {
    /* if we have a store ID, we're expecting name\0size\0 pairs */
    print_list_pairs(stdout, respmsg.entries.data, respmsg.entries.len);
  } else {
    /* without a store ID, we're just expecting name\0 entries */
    print_list_entries(stdout, respmsg.entries.data, respmsg.entries.len);
  }

  result = 0;
ycl_msg_cleanup:
  ycl_msg_cleanup(&msg);
  ycl_close(&ctx);
  return result;
}

static int put_main(int argc, char *argv[], int flags) {
  int ch;
  int ret;
  const char *optstr = "hs:i:n:";
  const char *socket = DFL_STOREPATH;
  const char *id = NULL;
  const char *filename = NULL;
  const char *name = NULL;
  struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"socket", required_argument, NULL, 's'},
    {"id", required_argument, NULL, 'i'},
    {"name", required_argument, NULL, 'n'},
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
    case 'n':
      name = optarg;
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
  ret = run_put(socket, id, name, filename, flags);
  if (ret < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
usage:
  fprintf(stderr, "usage: %s %s [opts] <name>\n"
      "opts:\n"
      "  -s|--socket <path>   store socket path (%s)\n"
      "  -i|--id     <id>     store ID\n"
      "  -n|--name   <name>   store name\n",
      argv[0], argv[1], DFL_STOREPATH);
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

static int run_index(const char *socket, size_t before, size_t nelems) {
  struct ycl_ctx ctx;
  struct ycl_msg msg;
  struct ycl_msg_store_req reqmsg = {{0}};
  int result = -1;
  int ret;
  int indexfd;
  FILE *fp;
  struct sindex_ctx si;
  ssize_t ss;
  struct sindex_entry *elems;
  size_t last = 0;
  size_t i;

  ret = setup_ycl_state(&ctx, socket, &msg);
  if (ret < 0) {
    return -1;
  }

  reqmsg.action.data = "index";
  reqmsg.action.len = sizeof("index") - 1;
  ret = ycl_msg_create_store_req(&msg, &reqmsg);
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

  ret = ycl_recvfd(&ctx, &indexfd);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_recvfd: %s\n", ycl_strerror(&ctx));
    goto ycl_msg_cleanup;
  }

  fp = fdopen(indexfd, "r");
  if (!fp) {
    close(indexfd);
    fprintf(stderr, "fdopen: %s\n", strerror(errno));
    goto ycl_msg_cleanup;
  }

  elems = calloc(nelems, sizeof(struct sindex_entry));
  if (!elems) {
    fprintf(stderr, "calloc: %s\n", strerror(errno));
    goto fp_cleanup;
  }

  sindex_init(&si, fp);
  ss = sindex_get(&si, elems, nelems, before, &last);
  if (ss < 0) {
    sindex_geterr(&si, databuf_, sizeof(databuf_));
    fprintf(stderr, "sindex_geterr: %s\n", databuf_);
    goto elems_cleanup;
  }

  for (i = 0; i < ss; i++) {
    elems[i].name[SINDEX_NAMESZ-1] = '\0';
    printf("%.*s %ld %zu %s\n", SINDEX_IDSZ, elems[i].id, elems[i].indexed,
        last++, elems[i].name);
  }

  result = 0;
elems_cleanup:
  free(elems);
fp_cleanup:
  fclose(fp);
ycl_msg_cleanup:
  ycl_msg_cleanup(&msg);
  ycl_close(&ctx);
  return result;
}

static int index_main(int argc, char *argv[]) {
  int ch;
  int ret;
  size_t before = 0;
  size_t nelems = DFL_INDEX_NELEMS;
  const char *socket = DFL_STOREPATH;
  const char *optstr = "hs:b:n:";
  struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"socket", required_argument, NULL, 's'},
    {"before", required_argument, NULL, 'b'},
    {"nelems", required_argument, NULL, 'n'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc-1, argv+1, optstr, longopts, NULL)) != -1) {
    switch(ch) {
    case 's':
      socket = optarg;
      break;
    case 'b':
      before = (size_t)strtoul(optarg, NULL, 10);
      break;
    case 'n':
      nelems = (size_t)strtoul(optarg, NULL, 10);
      break;
    case 'h':
    default:
      goto usage;
    }
  }

  ret = run_index(socket, before, nelems);
  if (ret < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
usage:
  fprintf(stderr, "usage: %s index [opts]\n"
      "opts:\n"
      "  -s|--socket   store socket path (%s)\n"
      "  -b|--before   where to page from\n"
      "  -n|--nelems   number of elements to get (%d)\n",
      argv[0], DFL_STOREPATH, DFL_INDEX_NELEMS);
  return EXIT_FAILURE;
}

int list_main(int argc, char *argv[]) {
  int ch;
  int ret;
  const char *optstr = "hs:m:";
  const char *socket = DFL_STOREPATH;
  const char *must_match = NULL;
  const char *id = NULL;
  struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"socket", required_argument, NULL, 's'},
    {"must-match", required_argument, NULL, 'm'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc-1, argv+1, optstr, longopts, NULL)) != -1) {
    switch(ch) {
    case 's':
      socket = optarg;
      break;
    case 'm':
      must_match = optarg;
      break;
    case 'h':
    default:
      goto usage;
    }
  }

  if (optind + 1 < argc) {
    id = argv[optind + 1];
  }

  ret = run_list(socket, id, must_match);
  if (ret < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
usage:
  fprintf(stderr, "usage: %s get [opts] [id]\n"
      "opts:\n"
      "  -s|--socket       store socket path (%s)\n"
      "  -m|--must-match   POSIX ERE for filtering the response\n",
      argv[0], DFL_STOREPATH);
  return EXIT_FAILURE;

}

int run_rename(const char *socket, const char *id, const char *from,
    const char *to) {
  int ret;
  int result = -1;
  struct ycl_ctx ctx;
  struct ycl_msg msg;
  struct ycl_msg_store_req reqmsg = {{0}};
  struct ycl_msg_status_resp respmsg = {{0}};
  struct ycl_msg_store_entered_req renamemsg = {{0}};

  ret = setup_ycl_state(&ctx, socket, &msg);
  if (ret < 0) {
    return -1;
  }

  reqmsg.action.data = "enter";
  reqmsg.action.len = sizeof("enter") - 1;
  reqmsg.store_id.data = id;
  reqmsg.store_id.len = id ? strlen(id) : 0;
  ret = ycl_msg_create_store_req(&msg, &reqmsg);
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

  renamemsg.action.data = "rename";
  renamemsg.action.len = 7;
  renamemsg.rename_from.data = from;
  renamemsg.rename_from.len = strlen(from);
  renamemsg.rename_to.data = to;
  renamemsg.rename_to.len = strlen(to);
  ret = ycl_msg_create_store_entered_req(&msg, &renamemsg);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to serialize rename request\n");
    goto ycl_msg_cleanup;
  }

  ret = ycl_sendmsg(&ctx, &msg);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to send rename request: %s\n", ycl_strerror(&ctx));
    goto ycl_msg_cleanup;
  }

  ycl_msg_reset(&msg);
  ret = ycl_recvmsg(&ctx, &msg);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to receive rename response: %s\n",
        ycl_strerror(&ctx));
    goto ycl_msg_cleanup;
  }

  memset(&respmsg, 0, sizeof(respmsg));
  ret = ycl_msg_parse_status_resp(&msg, &respmsg);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to parse rename response\n");
    goto ycl_msg_cleanup;
  }

  if (respmsg.errmsg.data != NULL && *respmsg.errmsg.data != '\0') {
    fprintf(stderr, "%s\n", respmsg.errmsg.data);
    goto ycl_msg_cleanup;
  }


  result = 0;
ycl_msg_cleanup:
  ycl_msg_cleanup(&msg);
  ycl_close(&ctx);
  return result;
}

int rename_main(int argc, char *argv[]) {
  int ch;
  int ret;
  const char *optstr = "hs:";
  const char *socket = DFL_STOREPATH;
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

  if (optind + 3 >= argc) {
    fprintf(stderr, "too few arguments");
    goto usage;
  }

  ret = run_rename(socket, argv[optind + 1], argv[optind + 2],
      argv[optind + 3]);
  if (ret < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
usage:
  fprintf(stderr, "usage: %s rename [opts] <id> <from> <to>\n"
      "opts:\n"
      "  -s|--socket       store socket path (%s)\n"
      "  -h|--help       this text\n",
      argv[0], DFL_STOREPATH);
  return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    goto usage;
  }

  if (strcmp(argv[1], "put") == 0) {
    return put_main(argc, argv, O_WRONLY | O_CREAT | O_TRUNC);
  } else if (strcmp(argv[1], "append") == 0) {
    return put_main(argc, argv, O_WRONLY | O_CREAT | O_APPEND);
  } else if (strcmp(argv[1], "get") == 0) {
    return get_main(argc, argv);
  } else if (strcmp(argv[1], "index") == 0) {
    return index_main(argc, argv);
  } else if (strcmp(argv[1], "list") == 0) {
    return list_main(argc, argv);
  } else if (strcmp(argv[1], "rename") == 0) {
    return rename_main(argc, argv);
  }

  fprintf(stderr, "unknown command\n");
usage:
  fprintf(stderr, "usage: %s <command> [args]\n"
      "commands:\n"
      "  put    - put a file in the store\n"
      "  append - append content to a file in a store\n"
      "  get    - get file content from a store\n"
      "  index  - retrieve indexed store entries\n"
      "  list   - list stores, store content\n"
      "  rename - renames a file in a store\n",
      argv[0]);
  return EXIT_FAILURE;
}
