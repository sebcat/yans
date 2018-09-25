#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <lib/net/reaplan.h>

#define DFL_SUBJECTFILE "-"
#define DFL_PORTS       "80,443"

struct opts {
  const char *subjectfile;
  const char *ports;
};

static int on_connect(struct reaplan_ctx *ctx, struct reaplan_conn *conn) {
  static int done = 0;
  if (!done) {
    conn->fd = STDIN_FILENO;
    conn->events = REAPLAN_READABLE;
    done = 1;
    return REAPLANC_OK;
  }

  return REAPLANC_DONE;
}

static int on_readable(struct reaplan_ctx *ctx, int fd) {
  ssize_t nread;
  char buf[512];

  nread = read(fd, buf, sizeof(buf));
  if (nread < 0) {
    return REAPLAN_ERR;
  } else if (nread > 0) {
    write(STDOUT_FILENO, buf, nread);
  }

  return REAPLAN_OK;
}

static int on_writable(struct reaplan_ctx *ctx, int fd) {
  return 0;
}

static int on_done(struct reaplan_ctx *ctx, int fd, int err) {
  printf("done fd:%d err:%d\n", fd, err);
  return REAPLAN_OK;
}

static void opts_or_die(struct opts *opts, int argc, char *argv[]) {
  /* fill in defaults */
  opts->subjectfile = DFL_SUBJECTFILE;
  opts->ports = DFL_PORTS;

  /* TODO: getopt_long */
}

int main(int argc, char *argv[]) {
  int ret;
  struct opts opts;
  struct reaplan_ctx reaplan;
  struct reaplan_opts rpopts = {
    .funcs = {
      .on_connect = on_connect,
      .on_readable = on_readable,
      .on_writable = on_writable,
      .on_done = on_done,
    },
    .data = NULL,
  };

  opts_or_die(&opts, argc, argv);

  ret = reaplan_init(&reaplan, &rpopts);
  if (ret != REAPLAN_OK) {
    return EXIT_FAILURE;
  }

  ret = reaplan_run(&reaplan);
  reaplan_cleanup(&reaplan);
  if (ret != REAPLAN_OK) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
