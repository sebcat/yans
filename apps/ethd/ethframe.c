#include <lib/util/netstring.h>
#include <lib/util/ylog.h>
#include <lib/util/io.h>

#include <proto/ethframe_req.h>

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

  IO_INIT(&io, fd);
  ret = io_readbuf(&io, &ecli->buf, NULL);
  if (ret == IO_AGAIN) {
    return;
  } else if (ret != IO_OK) {
    ylog_error("ethframecli%d: io_readbuf: %s", fd, io_strerror(&io));
    goto done;
  }

  if (ecli->buf.len >= MAX_CMDSZ) {
    ylog_error("ethframecli%d: maximum command size exceeded", fd);
    goto done;
  }

  ret = p_ethframe_req_deserialize(&req, ecli->buf.data, ecli->buf.len, NULL);
  if (ret == PROTO_ERRINCOMPLETE) {
    return;
  } else if (ret != PROTO_OK) {
    ylog_error("ethframecli%d: p_pcapd_cmd_deserialize: %s", fd,
        proto_strerror(ret));
    goto done;
  }

  if (req.iface == NULL) {
    ylog_error("ethframecli%d: no iface set", fd);
    goto done;
  }

  if (req.frameslen == 0) {
    ylog_error("ethframecli%d: no frames set", fd);
    goto done;
  }

  ylog_info("ethframecli%d: iface:\"%s\" frameslen:%zu", fd, req.iface,
      req.frameslen);

  /* XXX: eth_sender is blocking; we're assuming that it won't block
   *      for long and that the number of frames is small. We should consider
   *      making eth_sender non-blocking and allocating another eds_client for
   *      sending the frames, but only if the added complexity leads to a
   *      measurable, meaningful improvement */
  if (eth_sender_init(&sender, req.iface) < 0) {
    ylog_error("ethframecli%d: eth_sender_init: %s",
      fd, eth_sender_strerror(&sender));
    goto done;
  }

  frames = (char*)req.frames; /* XXX: casting away const, should be OK */
  while (req.frameslen > 0) {
    ret = netstring_next(&frame, &framelen, &frames, &req.frameslen);
    if (ret != NETSTRING_OK) {
      ylog_error("ethframecli%d: netstring_next: %s", fd,
          netstring_strerror(ret));
      eth_sender_cleanup(&sender);
      goto done;
    }

    if (eth_sender_write(&sender, frame, framelen) < 0) {
      ylog_error("ethframecli%d: eth_sender_write: %s", fd,
          eth_sender_strerror(&sender));
      eth_sender_cleanup(&sender);
      goto done;
    }
  }
  eth_sender_cleanup(&sender);

done:
  eds_client_clear_actions(cli);
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
