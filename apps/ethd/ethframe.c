#include <string.h>
#include <net/if.h>
#include <stdlib.h>
#include <limits.h>

#include <lib/util/netstring.h>
#include <lib/util/flagset.h>
#include <lib/util/ylog.h>
#include <lib/util/io.h>

#include <lib/net/iface.h>

#include <proto/ethframe_req.h>
#include <proto/status_resp.h>

#include <apps/ethd/ethframe.h>

#if ETH_ALEN != 6
#error "inconsistent ETH_ALEN length"
#endif

/* general settings */
#define INITREQBUFSZ       2048   /* initial buffer size for service request */
#define MAX_CMDSZ     (1 << 22)   /* maximum size of cmd request */
#define NBR_IFACES           32   /* maximum number of network interfaces */
#define YIELD_NPACKETS     1024   /* number of packets to send before yield */


/* frame_builder flags */
#define REQUIRES_IP4  (1 << 0) /* dst and src addrs must be IPv4 */
#define REQUIRES_IP6  (1 << 1) /* dst and src addrs must be IPv6 */
#define IP_DST_SWEEP  (1 << 2) /* advances the destination IP address */
#define IP_PORT_SWEEP (1 << 3) /* advances the destination port. Implies
                                  IP_DST_SWEEP */

/* frame categories */
#define CAT_ARP (1 << 0)

#define ETHFRAME_CLIENT(cli__) \
  ((struct ethframe_client *)((cli__)->udata))

struct ethframe_ctx {
  int nifs;
  char ifs[NBR_IFACES][IFNAMSIZ];
  struct eth_sender senders[NBR_IFACES];
};

static struct flagset_map category_flags[] = {
  FLAGSET_ENTRY("arpreqs", CAT_ARP),
  FLAGSET_END
};

/* process context */
static struct ethframe_ctx ethframe_ctx;

struct frame_builder {
  unsigned int category; /* what category flag(s) must be set */
  unsigned int options;  /* option flag(s) */
  const char *(*build)(struct frameconf *cfg, size_t *len);
};

static const char *gen_custom_frame(struct frameconf *cfg, size_t *len) {
  int ret;
  char *frame;
  size_t framelen;

  if (cfg->custom_frameslen == 0) {
    return NULL;
  }

  ret = netstring_next(&frame, &framelen, &cfg->custom_frames,
      &cfg->custom_frameslen);
  if (ret != NETSTRING_OK) {
    return NULL;
  }

  *len = framelen;
  return frame;
}

static const char *gen_arp_req(struct frameconf *cfg, size_t *len) {
  static char pkt[] = {
    /* Ethernet */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* dst */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* src */
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
    /* SPA (off: 28) */
    0x00, 0x00,
    0x00, 0x00,
    /* THA (N/A in request) (off: 32) */
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    /* TPA: addr to look up (off: 38) */
    0x00, 0x00,
    0x00, 0x00,
  };

  memcpy(pkt + 6, cfg->eth_src, ETH_ALEN);    /* eth source */
  memcpy(pkt + 22, cfg->eth_src, ETH_ALEN);   /* ARP SHA */
  *(uint32_t*)(pkt + 28) =
      cfg->src_ip.u.sin.sin_addr.s_addr; /* ARP SPA */
  *(uint32_t*)(pkt + 38) =
      cfg->curr_dst_ip.u.sin.sin_addr.s_addr; /* ARP TPA */
  *len = sizeof(pkt);
  return pkt;
}

static struct frame_builder frame_builders[] = {
  {
    .category = 0,
    .options = 0,
    .build = gen_custom_frame,
  },
  {
    .category = CAT_ARP,
    .options = IP_DST_SWEEP | REQUIRES_IP4,
    .build = gen_arp_req,
  }
};

static int is_family_allowed(struct frame_builder *fb, struct frameconf *cfg) {
  if (fb->options & REQUIRES_IP4  &&
      !(cfg->src_ip.u.sa.sa_family == AF_INET &&
        cfg->curr_dst_ip.u.sa.sa_family == AF_INET)) {
    return 0;
  } else if (fb->options & REQUIRES_IP6 &&
      !(cfg->src_ip.u.sa.sa_family == AF_INET6 &&
        cfg->curr_dst_ip.u.sa.sa_family == AF_INET6)) {
    return 0;
  }
  return 1;
}

static void next_builder(struct frameconf *cfg) {
  port_ranges_reset(&cfg->dst_ports);
  ip_blocks_reset(&cfg->dst_ips);
  cfg->curr_buildix++;
}

static const char *get_next_frame(struct frameconf *cfg, size_t *len) {
  struct frame_builder *fb;
  const char *ret;

again:
  if (cfg->curr_buildix >=
      sizeof(frame_builders) / sizeof(struct frame_builder)) {
    return NULL;
  }

  fb = frame_builders + cfg->curr_buildix;

  if (fb->category && !(fb->category & cfg->categories)) {
    next_builder(cfg);
    goto again;
  }

  /* advance addresses on sweep */
  if (fb->options & IP_PORT_SWEEP) {
    if (!port_ranges_next(&cfg->dst_ports, &cfg->curr_dst_port)) {
      if (!ip_blocks_next(&cfg->dst_ips, &cfg->curr_dst_ip)) {
        next_builder(cfg);
        goto again;
      }
      if (!is_family_allowed(fb, cfg)) {
        goto again;
      }
    }
  } else if (fb->options & IP_DST_SWEEP) {
    if (!ip_blocks_next(&cfg->dst_ips, &cfg->curr_dst_ip)) {
      next_builder(cfg);
      goto again;
    }
    if (!is_family_allowed(fb, cfg)) {
      goto again;
    }
  }

  ret = fb->build(cfg, len);
  if (ret == NULL) {
    next_builder(cfg);
    goto again;
  }

  return ret;
}


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
  struct iface_entry ent;
  int i;
  int ret;

  ret = iface_init(&ifs);
  if (ret < 0) {
    ylog_error("%s: iface_init: %s", svc->name, iface_strerror(&ifs));
    return -1;
  }

  for (i = 0; i < NBR_IFACES; i++) {
next_iface:
    ret = iface_next(&ifs, &ent);
    if (ret <= 0) {
      break;
    }

    /* skip loopbacks, interfaces like pflogN, &c */
    if (memcmp(ent.addr, "\0\0\0\0\0\0", IFACE_ADDRSZ) == 0) {
      goto next_iface;
    }

    if (eth_sender_init(&ethframe_ctx.senders[i], ent.name) < 0) {
      /* the interface may not be configured, so this should not be fatal */
      goto next_iface;
    }

    snprintf(ethframe_ctx.ifs[i], sizeof(ethframe_ctx.ifs[i]), "%s", ent.name);
    ethframe_ctx.nifs++;
    ylog_info("%s: initialized iface \"%s\"", svc->name, ent.name);
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
  buf_clear(&ecli->buf); /* reuse the request buffer */
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

static int setup_frameconf(struct frameconf *cfg, struct p_ethframe_req *req,
    char *errbuf, size_t errbuflen) {
  size_t failoff;
  struct flagset_result fsres = {0};
  unsigned long pps;
  char *end;

  /* XXX: we will mutate custom_frames, but it should reside in mutable memory,
   * so it should (...) be OK to cast away the const */
  cfg->custom_frames = (char*)req->custom_frames;
  cfg->custom_frameslen = req->custom_frameslen;

  if (req->categorieslen > 0) {
    if (flagset_from_str(category_flags, req->categories, &fsres) < 0) {
      snprintf(errbuf, errbuflen, "invalid categories, offset %zu: %s",
          fsres.erroff, fsres.errmsg);
      return -1;
    }
    cfg->categories = fsres.flags;
  }

  if (req->ppslen > 0) {
    pps = strtoul(req->pps, &end, 10);
    if (pps > UINT_MAX || *end != '\0') {
      snprintf(errbuf, errbuflen, "invalid pps value");
      return -1;
    }
    cfg->pps = (unsigned int)pps;
  }

  if (req->ifacelen == 0) {
    snprintf(errbuf, errbuflen, "missing iface");
    return -1;
  }
  cfg->iface = req->iface;

  if (req->eth_srclen > 0) {
    if (eth_parse_addr(cfg->eth_src, sizeof(cfg->eth_src), req->eth_src) < 0) {
      snprintf(errbuf, errbuflen, "invalid eth src");
      return -1;
    }
  } else if (cfg->custom_frameslen == 0 && cfg->categories != 0) {
    snprintf(errbuf, errbuflen, "missing eth src");
    return -1;
  }

  if (req->eth_dstlen > 0) {
    if (eth_parse_addr(cfg->eth_dst, sizeof(cfg->eth_dst), req->eth_dst) < 0) {
      snprintf(errbuf, errbuflen, "invalid eth dst");
      return -1;
    }
  }
  /* we don't require eth_dstlen to be set- the frames may have them (e.g.,
   * bcasts) */

  if (req->ip_srclen > 0) {
    if (ip_addr(&cfg->src_ip, req->ip_src, NULL) < 0) {
      snprintf(errbuf, errbuflen, "invalid ip src address");
      return -1;
    }
  } else if (cfg->custom_frameslen == 0 && cfg->categories != 0) {
    snprintf(errbuf, errbuflen, "missing ip src address");
    return -1;
  }

  if (req->ip_dstslen > 0) {
    if (ip_blocks_init(&cfg->dst_ips, req->ip_dsts, NULL) < 0) {
      snprintf(errbuf, errbuflen, "invalid ip dst addresses");
      return -1;
    }
  } else if (cfg->custom_frameslen == 0 && cfg->categories != 0) {
    snprintf(errbuf, errbuflen, "missing ip dst addresses");
    return -1;
  }

  if (req->port_dstslen > 0) {
    if (port_ranges_from_str(&cfg->dst_ports, req->port_dsts, &failoff) < 0) {
      snprintf(errbuf, errbuflen, "dst ports: syntax error near %zu", failoff);
      return -1;
    }
  }

  return 0;
}

static void cleanup_frameconf(struct frameconf *cfg) {
  ip_blocks_cleanup(&cfg->dst_ips);
  port_ranges_cleanup(&cfg->dst_ports);
}

/* returns -1 on failure, 0 when done, 1 if more things can be sent */
static int write_npackets(struct eds_client *cli, char *errbuf,
    size_t errbuflen) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  const char *frame;
  size_t framelen;
  int ret;

  while (ecli->npackets > 0) {
    frame = get_next_frame(&ecli->cfg, &framelen);
    if (frame == NULL) {
      return 0;
    }

    ret = eth_sender_write(ecli->sender, frame, framelen);
    if (ret < 0) {
      snprintf(errbuf, errbuflen, "error sending frame: %s",
          eth_sender_strerror(ecli->sender));
      return -1;
    }

    ecli->npackets--;
  }

  return 1;
}

static void on_write_frames_tick(struct eds_client *cli, int fd) {
  //struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  /* TODO: Implement */
  write_ok_response(cli, fd);
}

static void on_tick(struct eds_client *cli, int fd) {
  /* TODO: Implement */
}

static void on_write_frames_notick(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  char errbuf[128];
  int ret;

  ecli->npackets = YIELD_NPACKETS;
  ret = write_npackets(cli, errbuf, sizeof(errbuf));
  if (ret < 0) {
    write_err_response(cli, fd, errbuf);
  } else if (ret == 0) {
    write_ok_response(cli, fd);
  }
}

static void on_read_req(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  struct p_ethframe_req req;
  int ret;
  io_t io;
  size_t nread = 0;
  const char *errmsg = "an internal error occurred";
  char errbuf[128];

  IO_INIT(&io, fd);
  ret = io_readbuf(&io, &ecli->buf, &nread);
  if (ret == IO_AGAIN) {
    return;
  } else if (ret != IO_OK) {
    errmsg = io_strerror(&io);
    goto fail;
  }

  if (nread == 0) {
    errmsg = "connection closed while reading request";
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

  ret = setup_frameconf(&ecli->cfg, &req, errbuf, sizeof(errbuf));
  if (ret < 0) {
    errmsg = errbuf;
    goto fail;
  }

  ecli->sender = get_sender_by_ifname(ecli->cfg.iface);
  if (ecli->sender == NULL) {
    snprintf(errbuf, sizeof(errbuf), "iface \"%s\" does not exist", req.iface);
    errmsg = errbuf;
    goto fail;
  }

  ylog_info("ethframecli%d: iface:\"%s\"", fd, ecli->cfg.iface);

  eds_client_set_on_readable(cli, on_term, EDS_DEFER);

  if (ecli->cfg.pps > 0) {
    /* TODO: setup ticker */
    eds_client_set_ticker(cli, on_tick);
    eds_client_set_on_writable(cli, on_write_frames_tick, 0);
  } else {
    eds_client_set_on_writable(cli, on_write_frames_notick, 0);
  }
  return;

fail:
  write_err_response(cli, fd, errmsg);
}

void ethframe_on_readable(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);

  buf_init(&ecli->buf, INITREQBUFSZ);
  eds_client_set_on_readable(cli, on_read_req, 0);
}

void ethframe_on_done(struct eds_client *cli, int fd) {
  struct ethframe_client *ecli = ETHFRAME_CLIENT(cli);
  ylog_info("ethframecli%d: done", fd);
  cleanup_frameconf(&ecli->cfg);
  buf_cleanup(&ecli->buf);
}
