#include <string.h>
#include <net/if.h>

#include <lib/util/netstring.h>
#include <lib/util/ylog.h>
#include <lib/util/io.h>

#include <lib/net/iface.h>

#include <proto/ethframe_req.h>
#include <proto/status_resp.h>

#include <apps/ethd/ethframe.h>

#define INITREQBUFSZ       2048   /* initial buffer size for service request */
#define MAX_CMDSZ     (1 << 22)   /* maximum size of cmd request */
#define NBR_IFACES           32   /* maximum number of network interfaces */
#define YIELD_NPACKETS     1024   /* number of packets to send before yield */

#define ETHFRAME_CLIENT(cli__) \
  ((struct ethframe_client *)((cli__)->udata))


struct ethframe_ctx {
  int nifs;
  char ifs[NBR_IFACES][IFNAMSIZ];
  struct eth_sender senders[NBR_IFACES];
};

static struct ethframe_ctx ethframe_ctx;

static struct eth_sender *get_sender_by_ifname(const char *ifname) {
  int i;
  struct eth_sender *ret = NULL;

  for (i = 0; i < ethframe_ctx.nifs; i++) {
    if (strcmp(ethframe_ctx.ifs[i], ifname) == 0) {
      ret = &ethframe_ctx.senders[i];
      break;
    }
  }

  return ret;
}

int ethframe_init(struct eds_service *svc) {
  struct iface ifs;
  int i;
  int ret;
  const char *ifname;

  ret = iface_init(&ifs);
  if (ret < 0) {
    ylog_error("%s: iface_init: %s", svc->name, iface_strerror(&ifs));
    return -1;
  }

  for (i = 0; i < NBR_IFACES; i++) {
next_iface:
    ifname = iface_next(&ifs);
    if (ifname == NULL) {
      break;
    }

    if (eth_sender_init(&ethframe_ctx.senders[i], ifname) < 0) {
      /* the interface may not be configured, so this should not be fatal */
      goto next_iface;
    }

    snprintf(ethframe_ctx.ifs[i], sizeof(ethframe_ctx.ifs[i]), "%s", ifname);
    ethframe_ctx.nifs++;
    ylog_info("%s: initialized iface \"%s\"", svc->name, ifname);
  }

  iface_cleanup(&ifs);
  return 0;
}

void ethframe_fini(struct eds_service *svc) {
  int i;

  for (i = 0; i < ethframe_ctx.nifs; i++) {
    eth_sender_cleanup(&ethframe_ctx.senders[i]);
  }
}

static void write_ok_response(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  struct p_status_resp resp = {0};
  int ret;

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

  eds_client_send(cli, ecli->buf.data, ecli->buf.len, NULL);

}
static void write_err_response(struct eds_client *cli, int fd,
    const char *errmsg) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  struct p_status_resp resp = {0};
  int ret;

  ylog_error("ethframecli%d: %s", fd, errmsg);
  eds_client_clear_actions(cli);
  buf_clear(&ecli->buf);
  resp.errmsg = errmsg;
  resp.errmsglen = strlen(errmsg);
  ret = p_status_resp_serialize(&resp, &ecli->buf);
  if (ret != PROTO_OK) {
    ylog_error("ethframecli%d: OK response serialization error: %s", fd,
        proto_strerror(ret));
  } else {
    eds_client_send(cli, ecli->buf.data, ecli->buf.len, NULL);
  }
}

static void on_term(struct eds_client *cli, int fd) {
  eds_client_clear_actions(cli);
}

static int send_arp_req(struct eth_sender *sender, uint32_t spa,
    const char *sha, ip_addr_t *addr) {
  int ret = 0;
  static char pkt[] = {
    /* Ethernet */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* dst */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* src FIXME */
    0x08, 0x06, /* ethertype: ARP */

    /* ARP */
    0x00, 0x01, /* HTYPE: Ethernet */
    0x08, 0x00, /* PTYPE: IPv4 */
    0x06,       /* HLEN: 6*/
    0x04,       /* PLEN: 4*/
    0x00, 0x01, /* OPER: request */
    /* SHA (off: 22) */
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    /* SPA FIXME */
    0x00, 0x00,
    0x00, 0x00,
    /* THA (N/A in request) */
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    /* TPA: addr to look up (off: 38) */
    0x00, 0x00,
    0x00, 0x00,
  };

  if (addr->u.sin.sin_family == AF_INET) {
    memcpy(pkt + 6, sha, 6);      /* eth source */
    memcpy(pkt + 22, sha, 6);     /* ARP SHA */
    *(uint32_t*)(pkt + 28) = spa; /* ARP SPA */
    *(uint32_t*)(pkt + 38) = addr->u.sin.sin_addr.s_addr; /* ARP TPA */
    ret = eth_sender_write(sender, pkt, sizeof(pkt));
  }

  return ret;
}

static void on_write_arpreqs(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  size_t i;
  int ret;
  ip_addr_t addr;

  for (i = 0; i < YIELD_NPACKETS; i++) {
    ret = ip_blocks_next(&ecli->addrs, &addr);
    if (ret == 0) {
      /* we're done sending */
      write_ok_response(cli, fd);
      break;
    }

    ret = send_arp_req(ecli->sender, ecli->spa, ecli->sha, &addr);
    if (ret < 0) {
      write_err_response(cli, fd, eth_sender_strerror(ecli->sender));
      break;
    }
  }
}

static void on_read_req(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  struct p_ethframe_req req;
  int ret;
  io_t io;
  char *frame;
  char *frames;
  size_t framelen;
  const char *errmsg = "an internal error occurred";
  char errbuf[128];

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

  /* XXX: eth_sender is blocking; we're assuming that it won't block
   *      for long and that the number of frames is small. We should consider
   *      making eth_sender non-blocking and allocating another eds_client for
   *      sending the frames, but only if the added complexity leads to a
   *      measurable, meaningful improvement */
  ecli->sender = get_sender_by_ifname(req.iface);
  if (ecli->sender == NULL) {
    snprintf(errbuf, sizeof(errbuf), "iface \"%s\" does not exist", req.iface);
    errmsg = errbuf;
    goto fail;
  }

  if (req.arpreq_addrslen > 0) {
    /* if arpreq_addrs is set, arpreq_sha (source hardware address) and
     * arpreq_spa (source protocol address) must also be set explicitly.
     * we *could* obtain these from the interface, but it's better to be
     * explicit and let the caller deal with that */
    if (req.arpreq_shalen != 6) {
      errmsg = "missing or invalid SHA";
      goto fail;
    }
    memcpy(ecli->sha, req.arpreq_sha, req.arpreq_shalen);

    if (req.arpreq_spalen != 4) {
      errmsg = "missing or invalid SPA";
      goto fail;
    }
    ecli->spa = *(uint32_t*)req.arpreq_spa;

    ret = ip_blocks_init(&ecli->addrs, req.arpreq_addrs, NULL);
    if (ret < 0) {
      errmsg = "invalid address specification";
      goto fail;
    }
  }

  ylog_info("ethframecli%d: iface:\"%s\" frameslen:%zu arpreq_addrslen:%zu",
      fd, req.iface, req.frameslen, req.arpreq_addrslen);

  /* on readable at this point means session termination */
  eds_client_set_on_readable(cli, on_term);

  /* send custom frames, if any */
  frames = (char*)req.frames; /* XXX: casting away const (should be OK) */
  while (req.frameslen > 0) {
    ret = netstring_next(&frame, &framelen, &frames, &req.frameslen);
    if (ret != NETSTRING_OK) {
      errmsg = "invalid frame sent";
      goto fail;
    }

    if (eth_sender_write(ecli->sender, frame, framelen) < 0) {
      snprintf(errbuf, sizeof(errbuf), "frame send error: %s",
          eth_sender_strerror(ecli->sender));
      errmsg = errbuf;
      goto fail;
    }
  }

  /* TODO: perform ARP request sweep, if any. Since a sweep may take some
   *       time and block others from sending, we yield to the event loop every
   *       YIELD_NPACKETS packets. */
  if (req.arpreq_addrslen > 0) {
    /* NB: this is 'writable' on the client fd, not the packet sender. This
     *     is intentional, but non-intuitive. */
    eds_client_set_on_writable(cli, on_write_arpreqs);
    on_write_arpreqs(cli, fd);
    return;
  }

  write_ok_response(cli, fd);
  return;

fail:
  write_err_response(cli, fd, errmsg);
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
  ip_blocks_cleanup(&ecli->addrs);
}
