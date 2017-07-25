#include <lib/util/ylog.h>
#include <apps/ethd/sender.h>

int sender_init(struct eds_service *svc) {
  ylog_info("sender: inited");
  return 0;
}

void sender_fini(struct eds_service *svc) {
  ylog_info("sender: finalized");
}

void sender_on_readable(struct eds_client *cli, int fd) {
  eds_client_clear_actions(cli);
}

void sender_on_done(struct eds_client *cli, int fd) {
  ylog_info("sendercli%d: done", fd);
}
