#include <string.h>
#include <lib/util/ylog.h>
#include <lib/util/io.h>

#include <proto/sweeper_req.h>
#include <proto/status_resp.h>

#include <apps/ethd/sweeper.h>

#define MAX_CMDSZ (1 << 22)

#define SWEEPER_CLIENT(cli__) \
  ((struct sweeper_client *)((cli__)->udata))

static void on_tick(struct eds_client *cli, int fd) {
  struct sweeper_client *scli;
  struct p_status_resp resp = {0};
  int ret;

  scli = SWEEPER_CLIENT(cli);
  eds_client_set_ticker(cli, NULL);
  buf_clear(&scli->buf);
  resp.okmsg = "done";
  resp.okmsglen = 4;
  ret = p_status_resp_serialize(&resp, &scli->buf);
  if (ret != PROTO_OK) {
    ylog_error("sweepercli%d: OK response serialization error: %s", fd,
        proto_strerror(ret));
    return;
  }
  eds_client_send(cli, scli->buf.data, scli->buf.len, NULL);

}

static void on_term(struct eds_client *cli, int fd) {
  eds_client_clear_actions(cli);
}

static void on_read_req(struct eds_client *cli, int fd) {
  io_t io;
  int ret;
  char errbuf[256];
  struct sweeper_client *scli;
  const char *errmsg = "internal error";
  struct p_sweeper_req req;
  size_t nread = 0;
  struct p_status_resp resp = {0};
  struct eds_transition trans = {
    .flags = EDS_TFLREAD | EDS_TFLWRITE,
    .on_readable = NULL,
    .on_writable = NULL,
  };

  scli = SWEEPER_CLIENT(cli);
  IO_INIT(&io, fd);
  ret = io_readbuf(&io, &scli->buf, &nread);
  if (ret == IO_AGAIN) {
    return;
  } else if (ret != IO_OK) {
    errmsg = io_strerror(&io);
    goto fail;
  } else if (nread == 0) {
    errmsg = "premature connection termination";
    goto fail;
  }

  if (scli->buf.len >= MAX_CMDSZ) {
    errmsg = "maximum command size exceeded";
    goto fail;
  }

  ret = p_sweeper_req_deserialize(&req, scli->buf.data, scli->buf.len, NULL);
  if (ret == PROTO_ERRINCOMPLETE) {
    return;
  } else if (ret != PROTO_OK) {
    snprintf(errbuf, sizeof(errbuf), "request error: %s",
        proto_strerror(ret));
    errmsg = errbuf;
    goto fail;
  }

  if (req.addrs == NULL) {
    errmsg = "addrs not set";
    goto fail;
  }

  eds_client_set_ticker(cli, on_tick);
  eds_client_set_on_readable(cli, on_term);
  return;

fail:
  ylog_error("sweepercli%d: %s", fd, errmsg);
  eds_client_clear_actions(cli);
  buf_clear(&scli->buf);
  resp.errmsg = errmsg;
  resp.errmsglen = strlen(errmsg);
  ret = p_status_resp_serialize(&resp, &scli->buf);
  if (ret != PROTO_OK) {
    ylog_error("sweepercli%d: error response serialization error: %s", fd,
        proto_strerror(ret));
    return;
  }
  eds_client_send(cli, scli->buf.data, scli->buf.len, &trans);
}

void sweeper_on_readable(struct eds_client *cli, int fd) {
  struct sweeper_client *scli;
  scli = SWEEPER_CLIENT(cli);
  buf_init(&scli->buf, 2048);
  eds_client_set_on_readable(cli, on_read_req);
  on_read_req(cli, fd);
}

void sweeper_on_done(struct eds_client *cli, int fd) {
  struct sweeper_client *scli;
  scli = SWEEPER_CLIENT(cli);
  ylog_info("sweepercli%d: done", fd);
  buf_cleanup(&scli->buf);
}
