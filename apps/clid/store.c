#include <lib/util/ylog.h>
#include <apps/clid/store.h>

#define STOREFL_HASMSGBUF (1 << 0)

#define LOGERR(fd, ...) \
    ylog_error("storecli%d: %s", (fd), __VA_ARGS__)

#define LOGINFO(fd, ...) \
    ylog_info("storecli%d: %s", (fd), __VA_ARGS__)

static void on_readenter(struct eds_client *cli, int fd) {
  /* TODO: Implement */
  eds_service_remove_client(cli->svc, cli);
}

void store_on_readable(struct eds_client *cli, int fd) {
  struct store_cli *ecli = STORE_CLI(cli);
  int ret;

  ycl_init(&ecli->ycl, fd);
  if (ecli->flags & STOREFL_HASMSGBUF) {
    ycl_msg_reset(&ecli->msgbuf);
  } else {
    ret = ycl_msg_init(&ecli->msgbuf);
    if (ret != YCL_OK) {
      LOGERR(fd, "ycl_msg_init failure");
      goto fail;
    }
    ecli->flags |= STOREFL_HASMSGBUF;
  }

  eds_client_set_on_readable(cli, on_readenter, 0);
  return;
fail:
  eds_service_remove_client(cli->svc, cli);
}

void store_on_done(struct eds_client *cli, int fd) {
  struct store_cli *ecli = STORE_CLI(cli);
  /* TODO: Implement */
  (void)ecli;
  LOGINFO(fd, "done");
}

void store_on_finalize(struct eds_client *cli) {
  struct store_cli *ecli = STORE_CLI(cli);
  if (ecli->flags & STOREFL_HASMSGBUF) {
    ycl_msg_cleanup(&ecli->msgbuf);
    ecli->flags &= ~STOREFL_HASMSGBUF;
  }
}
