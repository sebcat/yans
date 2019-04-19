#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <lib/util/macros.h>
#include <lib/util/sandbox.h>
#include <lib/util/sindex.h>
#include <lib/ycl/yclcli_store.h>
#include <lib/ycl/opener.h>

#define DFL_INDEX_NELEMS 25

static char databuf_[32768]; /* get/put buffer */

static int setup_opener_state(struct opener_ctx *opener,
    struct opener_opts *opts, int sandbox) {
  int ret;

  ret = opener_init(opener, opts);
  if (ret < 0) {
    fprintf(stderr, "opener_init: %s\n", opener_strerror(opener));
    return -1;
  }

  if (sandbox) {
    ret = sandbox_enter();
    if (ret < 0) {
      fprintf(stderr, "sandbox_enter failure\n");
      opener_cleanup(opener);
      return -1;
    }
  } else {
    fprintf(stderr, "warning: sandbox disabled\n");
  }

  return 0;
}

static int setup_cli_state(struct yclcli_ctx *ctx, const char *socket,
    struct ycl_msg *msg) {
  int ret;

  ret = ycl_msg_init(msg);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_msg_init failure\n");
    goto fail;
  }

  yclcli_init(ctx, msg);
  ret = yclcli_connect(ctx, socket);
  if (ret != YCL_OK) {
    fprintf(stderr, "yclcli_connect: %s\n", yclcli_strerror(ctx));
    goto ycl_msg_cleanup;
  }

  ret = sandbox_enter();
  if (ret < 0) {
    fprintf(stderr, "sandbox_enter failure\n");
    goto yclcli_close;
  }

  return 0;
yclcli_close:
  yclcli_close(ctx);
ycl_msg_cleanup:
  ycl_msg_cleanup(msg);
fail:
return -1;
}

static int run_get(struct opener_opts *opts, const char *path,
    int sandbox) {
  int ret;
  int result = -1;
  FILE *getfp;
  struct opener_ctx opener;

  ret = setup_opener_state(&opener, opts, sandbox);
  if (ret < 0) {
    return -1;
  }

  ret = opener_fopen(&opener, path, "rb", &getfp);
  if (ret != YCL_OK) {
    fprintf(stderr, "%s: %s\n", path, opener_strerror(&opener));
    goto opener_cleanup;
  }

  for (;;) {
    size_t nread;
    size_t nwritten;
    char *curr;
    size_t left;

    nread = fread(databuf_, 1, sizeof(databuf_), getfp);
    if (nread == 0) {
      break;
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
          goto fclose_getfp;
        }
      } else if (nwritten == 0) {
        fprintf(stderr, "premature EOF\n");
        goto fclose_getfp;
      }
      left -= nwritten;
      curr += nwritten;
    }
  }

  result = 0;
fclose_getfp:
  fclose(getfp);
opener_cleanup:
  opener_cleanup(&opener);
  return result;
}

static int run_put(const char *socket, const char *id, const char *path,
    int flags) {
  int ret;
  int result = -1;
  int putfd = -1;
  struct ycl_msg msgbuf;
  struct yclcli_ctx cli;
  const char *set_id;

  ret = setup_cli_state(&cli, socket, &msgbuf);
  if (ret < 0) {
    return -1;
  }

  ret = yclcli_store_enter(&cli, id, &set_id);
  if (ret != YCL_OK) {
    fprintf(stderr, "yclcli_store_enter: %s\n", yclcli_strerror(&cli));
    goto ycl_msg_cleanup;
  }

  if (set_id && *set_id) {
    printf("%s\n", set_id);
  }

  /* set_id becomes invalid at the next call to storecli - don't keep any
   * dangling pointers */
  set_id = NULL;

  ret = yclcli_store_open(&cli, path, flags, &putfd);
  if (ret != YCL_OK) {
    fprintf(stderr, "%s: %s\n", path, yclcli_strerror(&cli));
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
  yclcli_close(&cli);
  ycl_msg_cleanup(&msgbuf);
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
  struct ycl_msg msg;
  struct yclcli_ctx cli;
  const char *resp = NULL;
  size_t resplen = 0;

  ret = setup_cli_state(&cli, socket, &msg);
  if (ret < 0) {
    return -1;
  }

  ret = yclcli_store_list(&cli, id, must_match, &resp, &resplen);
  if (ret != YCL_OK) {
    fprintf(stderr, "yclcli_store_ĺist: %s\n", yclcli_strerror(&cli));
    goto ycl_msg_cleanup;
  }

  if (id && *id) {
    /* if we have a store ID, we're expecting name\0size\0 pairs */
    print_list_pairs(stdout, resp, resplen);
  } else {
    /* without a store ID, we're just expecting name\0 entries */
    print_list_entries(stdout, resp, resplen);
  }

  result = 0;
ycl_msg_cleanup:
  yclcli_close(&cli);
  ycl_msg_cleanup(&msg);
  return result;
}

static int _put_main(int argc, char *argv[], int flags) {
  int ch;
  int ret;
  const char *optstr = "hs:i:";
  const char *socket = STORECLI_DFLPATH;
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
  ret = run_put(socket, id, filename, flags);
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
      argv[0], argv[1], STORECLI_DFLPATH);
  return EXIT_FAILURE;
}

static int put_main(int argc, char *argv[]) {
  return _put_main(argc, argv, O_WRONLY | O_CREAT | O_TRUNC);
}

static int append_main(int argc, char *argv[]) {
  return _put_main(argc, argv, O_WRONLY | O_CREAT | O_APPEND);
}


static int get_main(int argc, char *argv[]) {
  int ch;
  int ret;
  int sandbox = 1;
  struct opener_opts opts = {
    .socket = STORECLI_DFLPATH,
  };
  const char *optstr = "hs:X";
  const char *filename = NULL;
  struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"socket", required_argument, NULL, 's'},
    {"no-sandbox", no_argument, NULL, 'X'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc-1, argv+1, optstr, longopts, NULL)) != -1) {
    switch(ch) {
    case 's':
      opts.socket = optarg;
      break;
    case 'X':
      sandbox = 0;
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

  opts.store_id = getenv("YANS_ID");
  ret = run_get(&opts, filename, sandbox);
  if (ret < 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
usage:
  fprintf(stderr, "usage: %s get [opts] <name>\n"
      "opts:\n"
      "  -s|--socket       store socket path (%s)\n"
      "  -X|--no-sandbox   disable sandbox\n"
      , argv[0], STORECLI_DFLPATH);
  return EXIT_FAILURE;
}

static int run_index(const char *socket, size_t before, size_t nelems) {
  struct ycl_msg msgbuf;
  struct yclcli_ctx cli;
  int result = -1;
  int ret;
  int indexfd;
  FILE *fp;
  struct sindex_ctx si;
  ssize_t ss;
  struct sindex_entry *elems;
  size_t last = 0;
  size_t i;

  ret = setup_cli_state(&cli, socket, &msgbuf);
  if (ret < 0) {
    return -1;
  }

  ret = yclcli_store_index(&cli, before, nelems, &indexfd);
  if (ret != YCL_OK) {
    fprintf(stderr, "yclcli_store_index: %s\n", yclcli_strerror(&cli));
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
  yclcli_close(&cli);
  ycl_msg_cleanup(&msgbuf);
  return result;
}

static int index_main(int argc, char *argv[]) {
  int ch;
  int ret;
  size_t before = 0;
  size_t nelems = DFL_INDEX_NELEMS;
  const char *socket = STORECLI_DFLPATH;
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
      argv[0], STORECLI_DFLPATH, DFL_INDEX_NELEMS);
  return EXIT_FAILURE;
}

int list_main(int argc, char *argv[]) {
  int ch;
  int ret;
  const char *optstr = "hs:m:";
  const char *socket = STORECLI_DFLPATH;
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
  fprintf(stderr, "usage: %s list [opts] [id]\n"
      "opts:\n"
      "  -s|--socket       store socket path (%s)\n"
      "  -m|--must-match   POSIX ERE for filtering the response\n",
      argv[0], STORECLI_DFLPATH);
  return EXIT_FAILURE;

}

int run_rename(const char *socket, const char *id, const char *from,
    const char *to) {
  int ret;
  int result = -1;
  struct ycl_msg msgbuf;
  struct yclcli_ctx cli;

  ret = setup_cli_state(&cli, socket, &msgbuf);
  if (ret < 0) {
    return -1;
  }

  ret = yclcli_store_enter(&cli, id, NULL);
  if (ret != YCL_OK) {
    fprintf(stderr, "yclcli_store_enter: %s\n", yclcli_strerror(&cli));
    goto ycl_msg_cleanup;
  }

  ret = yclcli_store_rename(&cli, from, to);
  if (ret != YCL_OK) {
    fprintf(stderr, "yclcli_store_rename: %s\n", yclcli_strerror(&cli));
    goto ycl_msg_cleanup;
  }

  result = 0;
ycl_msg_cleanup:
  yclcli_close(&cli);
  ycl_msg_cleanup(&msgbuf);
  return result;
}

int rename_main(int argc, char *argv[]) {
  int ch;
  int ret;
  const char *optstr = "hs:";
  const char *socket = STORECLI_DFLPATH;
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
      argv[0], STORECLI_DFLPATH);
  return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
  size_t i;
  static const struct {
    const char *name;
    int (*func)(int, char **);
    const char *desc;
  } cmds[] = {
    {"put", put_main, "put a file in the store"},
    {"append", append_main, "append content to a file in a store"},
    {"get", get_main, "get file content from a store"},
    {"index", index_main, "retrieve indexed store entries"},
    {"list", list_main, "list stores, store content"},
    {"rename", rename_main, "renames a file in a store"}
  };

  if (argc < 2) {
    goto usage;
  }

  for (i = 0; i < ARRAY_SIZE(cmds); i++) {
    if (strcmp(argv[1], cmds[i].name) == 0) {
      return cmds[i].func(argc, argv);
    }
  }

usage:
  fprintf(stderr, "usage: %s <command> [args]\n"
      "commands:\n", argv[0]);
  for (i = 0; i < ARRAY_SIZE(cmds); i++) {
    fprintf(stderr, "  %s\n    %s\n", cmds[i].name, cmds[i].desc);
  }

  return EXIT_FAILURE;
}
