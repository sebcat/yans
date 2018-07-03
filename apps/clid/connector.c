#include <unistd.h>

#include <lib/util/eds.h>
#include <lib/util/ylog.h>
#include <lib/util/nullfd.h>
#include <lib/net/sconn.h>

#include <apps/clid/connector.h>

#define CONNECTOR_CLI(cli__) \
    (struct connector_cli*)((cli__)->udata)

#define CONNECTORFL_HASMSGBUF (1 << 0)

#define CONNECTOR_MAX_RETRIES 10

#define LOGERR(...) \
    ylog_error(__VA_ARGS__)

#define LOGINFO(...) \
    ylog_info(__VA_ARGS__)

static void on_readreq(struct eds_client *cli, int fd);

static int start_nonblock_connect(struct ycl_msg_connector_req *req,
    int *outerr) {
  int ret;
  struct sconn_ctx ctx = {0};
  struct sconn_opts opts = {
    .reuse_addr = (int)req->reuse_addr,
    .proto = req->proto.data,
    .bindaddr = req->bindaddr.data,
    .bindport = req->bindport.data,
    .dstaddr = req->dstaddr.data,
    .dstport = req->dstport.data,
  };

  /* setup nretries if it's in a sane range, for an arbitrary definition of
   * "sane" */
  if (req->nretries > 0 && req->nretries < CONNECTOR_MAX_RETRIES) {
    opts.nretries = (int)req->nretries;
  }

  ret = sconn_connect(&ctx, &opts);
  if (ret < 0 && outerr != NULL) {
    *outerr = sconn_errno(&ctx);
  }

  return ret;
}

static void on_sendfd(struct eds_client *cli, int fd) {
  struct connector_cli *ecli = CONNECTOR_CLI(cli);
  int ret;

  ret = ycl_sendfd(&ecli->ycl, ecli->connfd, ecli->connerr);
  if (ret == YCL_AGAIN) {
    return;
  } else if (ret != YCL_OK) {
    LOGERR("connectorcli%d: ycl_sendfd: %s", fd, ycl_strerror(&ecli->ycl));
    eds_client_clear_actions(cli);
    goto done;
  }

  eds_client_set_on_readable(cli, on_readreq, EDS_DEFER);
  eds_client_set_on_writable(cli, NULL, 0);

done:
  if (ecli->connfd >= 0 && ecli->connfd != nullfd_get()) {
    close(ecli->connfd);
  }
}

static void on_readreq(struct eds_client *cli, int fd) {
  struct connector_cli *ecli = CONNECTOR_CLI(cli);
  struct ycl_msg_connector_req req = {0};
  int ret;
  int connfd;
  int connerr = 0;

  ret = ycl_recvmsg(&ecli->ycl, &ecli->msgbuf);
  if (ret == YCL_AGAIN) {
    return;
  } else if (ret != YCL_OK) {
    eds_client_clear_actions(cli);
    return;
  }

  ret = ycl_msg_parse_connector_req(&ecli->msgbuf, &req);
  if (ret != YCL_OK) {
    LOGERR("connectorcli%d: connector_req parse error", fd);
    goto fail;
  }

  connfd = start_nonblock_connect(&req, &connerr);
  if (connfd < 0) {
    ecli->connfd = nullfd_get();
    ecli->connerr = connerr;
  } else {
    ecli->connfd = connfd;
    ecli->connerr = 0;
  }

  eds_client_set_on_writable(cli, on_sendfd, 0);
  eds_client_set_on_readable(cli, NULL, 0);
  return;

fail:
  eds_client_clear_actions(cli);
}

void connector_on_readable(struct eds_client *cli, int fd) {
  struct connector_cli *ecli = CONNECTOR_CLI(cli);
  int ret;

  ycl_init(&ecli->ycl, fd);
  if (ecli->flags & CONNECTORFL_HASMSGBUF) {
    ycl_msg_reset(&ecli->msgbuf);
  } else {
    ret = ycl_msg_init(&ecli->msgbuf);
    if (ret != YCL_OK) {
      LOGERR("connectorcli%d: ycl_msg_init failure", fd);
      goto fail;
    }
    ecli->flags |= CONNECTORFL_HASMSGBUF;
  }

  eds_client_set_on_readable(cli, on_readreq, 0);
  return;

fail:
  eds_client_clear_actions(cli);
}

void connector_on_done(struct eds_client *cli, int fd) {
  LOGINFO("connectorcli%d: done", fd);
}

void connector_on_finalize(struct eds_client *cli) {
  struct connector_cli *ecli = CONNECTOR_CLI(cli);
  if (ecli->flags & CONNECTORFL_HASMSGBUF) {
    ycl_msg_cleanup(&ecli->msgbuf);
    ecli->flags &= ~CONNECTORFL_HASMSGBUF;
  }
}
