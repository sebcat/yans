#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <apps/scan/resolve.h>
#include <apps/scan/banners.h>
#include <apps/scan/scan.h>

static struct scan_ctx scan_;
static struct scan_cmd cmds_[] = {
  {"resolve", resolve_main, "Resolve any names in a set of host-specs"},
  {"banners", banners_main, "Grab banners from remote destinations"},
};
static size_t ncmds_ = sizeof(cmds_) / sizeof(*cmds_);

int main(int argc, char *argv[]) {
  struct opener_opts opts_opener = {0};
  int status = EXIT_FAILURE;
  struct opener_ctx *opener;
  int ret;
  int i;

  if (argc < 2 || strcmp(argv[1], "-h") == 0 ||
      strcmp(argv[1], "--help") == 0) {
    goto usage;
  }

  ret = ycl_msg_init(&scan_.msgbuf);
  if (ret < 0) {
    fprintf(stderr, "failed to initialize ycl message buffer\n");
    goto end;
  }

  /* if YANS_ID is set - we use a store for our output. If not, we use the
   * filesystem directly */
  opts_opener.store_id = getenv("YANS_ID");
  opts_opener.msgbuf = &scan_.msgbuf;
  opener = &scan_.opener;
  ret = opener_init(opener, &opts_opener);
  if (ret < 0) {
    fprintf(stderr, "opener_init failure: %s\n", opener_strerr(opener));
    goto ycl_msg_cleanup;
  }

  /* ignore SIGPIPE, caused by writes on a file descriptor where the peer
   * has closed the connection */
  signal(SIGPIPE, SIG_IGN);

  /* lookup the command in argv[1] and call its callback, if any. If no
   * callback is found then status will be EXIT_FAILURE */
  for (i = 0; i < ncmds_; i++) {
    if (strcmp(cmds_[i].name, argv[1]) == 0) {
      status = cmds_[i].func(&scan_, argc, argv);
      break;
    }
  }

  if (i == ncmds_) {
    fprintf(stderr, "unknown command: %s\n", argv[1]);
  }

  opener_cleanup(opener);
ycl_msg_cleanup:
  ycl_msg_cleanup(&scan_.msgbuf);
end:
  return status;

usage:
  fprintf(stderr,
      "%s <command> [options]\n"
      "commands:\n"
      , argv[0]);
  for (i = 0; i < ncmds_ ; i++) {
    fprintf(stderr, "  %s - %s\n", cmds_[i].name, cmds_[i].desc);
  }

  return EXIT_FAILURE;
}
