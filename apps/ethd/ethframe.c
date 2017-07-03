#include <string.h>

#include <lib/util/netstring.h>
#include <lib/util/ylog.h>
#include <lib/util/io.h>

#include <proto/ethframe_req.h>
#include <proto/status_resp.h>

#include <apps/ethd/ethframe.h>

#define INITREQBUFSZ 2048     /* initial buffer size for service request */
#define MAX_CMDSZ (1 << 22)   /* maximum size of cmd request */

#define ETHFRAME_CLIENT(cli__) \
  ((struct ethframe_client *)((cli__)->udata))

static void on_read_req(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  struct eth_sender sender;
  struct p_ethframe_req req;
  int ret;
  io_t io;
  char *frame;
  char *frames;
  size_t framelen;
  const char *errmsg = "an internal error occurred";
  struct p_status_resp resp = {0};
  char errbuf[128];
  struct eds_transition trans = {
    .flags = EDS_TFLREAD | EDS_TFLWRITE,
    .on_readable = NULL,
    .on_writable = NULL,
  };

  IO_INIT(&io, fd);
  ret = io_readbuf(&io, &ecli->buf, NULL);
  if (ret == IO_AGAIN) {
    return;
  } else if (ret != IO_OK) {
    errmsg = io_strerror(&io);
    goto fail;
  }

  if (ecli->buf.len >= MAX_CMDSZ) {
    errmsg = "maximum command size exceeded";
    goto fail;
  }

  ret = p_ethframe_req_deserialize(&req, ecli->buf.data, ecli->buf.len, NULL);
  if (ret == PROTO_ERRINCOMPLETE) {
    return;
  } else if (ret != PROTO_OK) {
    snprintf(errbuf, sizeof(errbuf), "request error: %s",
        proto_strerror(ret));
    errmsg = errbuf;
    goto fail;
  }

  if (req.iface == NULL) {
    errmsg = "no iface set";
    goto fail;
  }

  if (req.frameslen == 0) {
    errmsg = "no frames set";
    goto fail;
  }

  ylog_info("ethframecli%d: iface:\"%s\" frameslen:%zu", fd, req.iface,
      req.frameslen);

  /* XXX: eth_sender is blocking; we're assuming that it won't block
   *      for long and that the number of frames is small. We should consider
   *      making eth_sender non-blocking and allocating another eds_client for
   *      sending the frames, but only if the added complexity leads to a
   *      measurable, meaningful improvement */
  if (eth_sender_init(&sender, req.iface) < 0) {
    snprintf(errbuf, sizeof(errbuf), "sender init error: iface:\"%s\" %s",
        req.iface, eth_sender_strerror(&sender));
    errmsg = errbuf;
    goto fail;
  }

  /* XXX: casting away const, should be OK in this context */
  frames = (char*)req.frames;
  while (req.frameslen > 0) {
    ret = netstring_next(&frame, &framelen, &frames, &req.frameslen);
    if (ret != NETSTRING_OK) {
      errmsg = "invalid frame sent";
      eth_sender_cleanup(&sender);
      goto fail;
    }

    if (eth_sender_write(&sender, frame, framelen) < 0) {
      snprintf(errbuf, sizeof(errbuf), "frame send error: %s",
          eth_sender_strerror(&sender));
      errmsg = errbuf;
      eth_sender_cleanup(&sender);
      goto fail;
    }
  }
  eth_sender_cleanup(&sender);
  eds_client_clear_actions(cli);
  buf_clear(&ecli->buf);
  resp.okmsg = "ok";
  resp.okmsglen = sizeof("ok")-1;
  ret = p_status_resp_serialize(&resp, &ecli->buf);
  if (ret != PROTO_OK) {
    ylog_error("ethframecli%d: error response serialization error: %s", fd,
        proto_strerror(ret));
    return;
  }

  eds_client_send(cli, ecli->buf.data, ecli->buf.len, &trans);
  return;

fail:
  ylog_error("ethframecli%d: %s", fd, errmsg);
  eds_client_clear_actions(cli);
  buf_clear(&ecli->buf);
  resp.errmsg = errmsg;
  resp.errmsglen = strlen(errmsg);
  ret = p_status_resp_serialize(&resp, &ecli->buf);
  if (ret != PROTO_OK) {
    ylog_error("ethframecli%d: OK response serialization error: %s", fd,
        proto_strerror(ret));
    return;
  }

  eds_client_send(cli, ecli->buf.data, ecli->buf.len, &trans);
}

void ethframe_on_readable(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);

  buf_init(&ecli->buf, INITREQBUFSZ);
  eds_client_set_on_readable(cli, on_read_req);
  on_read_req(cli, fd);
}

void ethframe_on_done(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  ylog_info("ethframecli%d: done", fd);
  buf_cleanup(&ecli->buf);
}
