#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>

#include <lib/util/io.h>
#include <lib/util/ylog.h>
#include <lib/util/netstring.h>

#include <lib/ycl/ycl_msg.h>

#include <apps/ethd/pcap.h>

#define CMDBUFINITSZ 1024     /* initial size of allocated cmd buffer */
#define SNAPLEN 2048          /* pcap_open_live snapshot length */
#define PCAP_TO_MS 1000       /* pcap_open_live timeout, in ms */
#define PCAP_DISPATCH_CNT 64  /* pcap_dispatch dispatch count */

/* struct pcap_client flags */
#define FLAGS_HASMSGBUF (1 << 0)

enum resptype {
  RESPTYPE_OK,
  RESPTYPE_ERR,
};

static void on_read_fd(struct eds_client *cli, int fd);

static void cleanup_capture(struct eds_client *cli) {
  struct pcap_client *pcapcli = PCAP_CLIENT(cli);

  if (pcapcli->dumpf != NULL) {
    fclose(pcapcli->dumpf);
    pcapcli->dumpf = NULL;
  }
  if (pcapcli->pcap != NULL) {
    pcap_close(pcapcli->pcap);
    pcapcli->pcap = NULL;
  }
  if (pcapcli->dumper != NULL) {
    pcap_dump_close(pcapcli->dumper);
    pcapcli->dumper = NULL;
  }
  if (pcapcli->dumpcli != NULL) {
    eds_service_remove_client(cli->svc, pcapcli->dumpcli);
    pcapcli->dumpcli = NULL;
  }
}

static void on_after_capture(struct eds_client *cli, int fd) {
  struct pcap_client *pcapcli = PCAP_CLIENT(cli);
  cleanup_capture(cli);
  ycl_msg_reset(&pcapcli->common.msgbuf);
  eds_client_set_on_readable(cli, on_read_fd, 0);
}

static void sendresp(struct eds_client *cli, enum resptype t,
    const char *msg) {
  struct ycl_msg_status_resp resp = {0};
  struct eds_transition trans;
  struct pcap_client *pcapcli = PCAP_CLIENT(cli);
  int ret;

  /* XXX: It's *very* important that the eds_client here is a pcap_client
   *      and not the dumper client. maybe assert a magic number here */

  /* empty messages are not allowed */
  assert(msg != NULL && *msg != '\0');

  /* set the transition and response fields */
  if (t == RESPTYPE_OK) {
    trans.flags = EDS_TFLWRITE;
    trans.on_writable = NULL;
    resp.okmsg = msg;
  } else {
    eds_client_set_on_readable(cli, NULL, 0); /* stop any reader routines */
    trans.flags = EDS_TFLREAD | EDS_TFLWRITE;
    trans.on_readable = NULL;
    trans.on_writable = NULL;
    resp.errmsg = msg;
  }

  /* serialize the response */
  ycl_msg_reset(&pcapcli->common.msgbuf); /* reuse msg for responses */
  ret = ycl_msg_create_status_resp(&pcapcli->common.msgbuf, &resp);
  if (ret != YCL_OK) {
    ylog_error("pcapcli%d: sendresp serialization error",
        eds_client_get_fd(cli));
    eds_client_clear_actions(cli);
    return;
  }

  eds_client_send(cli, ycl_msg_bytes(&pcapcli->common.msgbuf),
      ycl_msg_nbytes(&pcapcli->common.msgbuf), &trans);
}

static void on_gotpkg(struct eds_client *cli, int fd) {
  struct pcap_client_dumper *dumpcli;
  struct eds_client *parentcli;
  struct pcap_client *pcapcli;

  dumpcli = PCAP_CLIENT_DUMPER(cli);
  parentcli = dumpcli->parent;
  pcapcli = PCAP_CLIENT(parentcli);
  if (pcap_dispatch(pcapcli->pcap, PCAP_DISPATCH_CNT, pcap_dump,
      (u_char*)pcapcli->dumper) < 0) {
    ylog_error("pcapcli%d: pcap_dispatch: %s", eds_client_get_fd(parentcli),
        pcap_geterr(pcapcli->pcap));
    eds_client_clear_actions(parentcli);
    return;
  }
}

static void dumpcli_set_parent(struct eds_client *cli,
    struct eds_client *parent) {
  struct pcap_client_dumper *dumpcli = PCAP_CLIENT_DUMPER(cli);
  dumpcli->parent = parent;
}

static void on_readreq(struct eds_client *cli, int fd) {
  struct pcap_client *pcapcli = PCAP_CLIENT(cli);
  struct eds_client *dumpcli;
  struct eds_client_actions acts = {0};
  struct ycl_msg_pcap_req req = {0};
  char errbuf[PCAP_ERRBUF_SIZE];
  struct bpf_program bpf = {0};
  int ret;
  int pcapfd;
  const char *errmsg = "an internal error occurred";

  ret = ycl_recvmsg(&pcapcli->ycl, &pcapcli->common.msgbuf);
  if (ret == YCL_AGAIN) {
    return;
  } else if (ret != YCL_OK) {
    ylog_error("pcapcli%d: ycl_recvmsg: %s", fd, ycl_strerror(&pcapcli->ycl));
    goto fail;
  }

  ret = ycl_msg_parse_pcap_req(&pcapcli->common.msgbuf, &req);
  if (ret != YCL_OK) {
    ylog_error("pcapcli%d: invalid request message", fd);
    goto fail;
  }

  if (req.iface == NULL) {
    ylog_error("pcapcli%d: iface not set", fd);
    errmsg = "no interface specified";
    goto fail;
  }

  ylog_info("pcapcli%d: iface:\"%s\" %s filter", fd, req.iface,
      req.filter == NULL ? "without" : "with");

  errbuf[0] = '\0';
  if ((pcapcli->pcap = pcap_open_live(req.iface, SNAPLEN, 0,
      PCAP_TO_MS, errbuf)) == NULL) {
    ylog_error("pcapcli%d: pcap_open_live: %s", fd, errbuf);
    snprintf(pcapcli->msg, sizeof(pcapcli->msg), "%s", errbuf);
    errmsg = pcapcli->msg;
    goto fail;
  } else if (errbuf[0] != '\0') {
    ylog_info("pcapcli%d: pcap_open_live warning: %s", fd, errbuf);
  }

  if (req.filter != NULL) {
    if (pcap_compile(pcapcli->pcap, &bpf, req.filter, 1,
        PCAP_NETMASK_UNKNOWN) < 0) {
      ylog_error("pcapcli%d: pcap_compile: %s", fd,
          pcap_geterr(pcapcli->pcap));
      errmsg = "filter compilation failure";
      goto fail;
    }
    ret = pcap_setfilter(pcapcli->pcap, &bpf);
    pcap_freecode(&bpf);
    if (ret < 0) {
      ylog_error("pcapcli%d: pcap_setfilter: %s", fd,
          pcap_geterr(pcapcli->pcap));
      goto fail;
    }
  }

  if ((pcapcli->dumper = pcap_dump_fopen(pcapcli->pcap,
      pcapcli->dumpf)) == NULL) {
    ylog_error("pcapcli%d: pcap_dump_fopen: %s", fd,
        pcap_geterr(pcapcli->pcap));
    goto fail;
  }

  pcapcli->dumpf = NULL; /* pcapcli->dumper has ownership over pcapcli->dumpf,
                          * and will clear it on pcap_dump_close */
  if (pcap_setnonblock(pcapcli->pcap, 1, errbuf) < 0) {
    ylog_error("pcapcli%d: pcap_setnonblock: %s", fd, errbuf);
    goto fail;
  }

  if ((pcapfd = pcap_get_selectable_fd(pcapcli->pcap)) < 0) {
    ylog_error("pcapcli%d: pcap_get_selectable_fd failure", fd);
    goto fail;
  }

  acts.on_readable = on_gotpkg;
  acts.on_finalize = pcap_on_finalize;
  if ((dumpcli = eds_service_add_client(cli->svc, pcapfd, &acts)) == NULL) {
    ylog_error("pcapcli%d: eds_service_add_client failure", fd);
    goto fail;
  }

  dumpcli_set_parent(dumpcli, cli);
  eds_client_set_externalfd(dumpcli);
  pcapcli->dumpcli = dumpcli;

  /* send OK response and enter on_after_capture at next readable event */
  sendresp(cli, RESPTYPE_OK, "started");
  eds_client_set_on_readable(cli, on_after_capture, EDS_DEFER);
  return;

fail:
  eds_client_clear_actions(cli);
  sendresp(cli, RESPTYPE_ERR, errmsg);
}

static void on_read_fd(struct eds_client *cli, int fd) {
  struct pcap_client *pcapcli = PCAP_CLIENT(cli);
  int pcapfd;
  FILE *fp;

  if (ycl_recvfd(&pcapcli->ycl, &pcapfd) != YCL_OK) {
    /* this could be erroneous, but it could also be a successful shutdown */
    goto done;
  }

  fp = fdopen(pcapfd, "w");
  if (fp == NULL) {
    ylog_error("pcapcli%d: fdopen: %s", fd, strerror(errno));
    goto cleanup_pcapfd;
  }

  pcapcli->dumpf = fp; /* will be closed by on_done */
  eds_client_set_on_readable(cli, on_readreq, 0);
  return;

cleanup_pcapfd:
  close(pcapfd);
done:
  eds_client_clear_actions(cli);
}

/* initial on_readable event */
void pcap_on_readable(struct eds_client *cli, int fd) {
  struct pcap_client *pcapcli = PCAP_CLIENT(cli);
  int ret;

  ycl_init(&pcapcli->ycl, fd);
  /* if this is the first time we run on this client, allocate the
   * ycl_msg buffer, which will be reused for subsequent clients */
  if (!(pcapcli->common.flags & FLAGS_HASMSGBUF)) {
    ret = ycl_msg_init(&pcapcli->common.msgbuf);
    if (ret != YCL_OK) {
      ylog_error("pcapcli%d: ycl_msg_init failure", fd);
      goto fail;
    }
    pcapcli->common.flags |= FLAGS_HASMSGBUF;
  } else {
    ycl_msg_reset(&pcapcli->common.msgbuf);
  }

  eds_client_set_on_readable(cli, on_read_fd, 0);
  return;

fail:
  eds_client_clear_actions(cli);
}

void pcap_on_done(struct eds_client *cli, int fd) {
  ylog_info("pcapcli%d: done", fd);
  cleanup_capture(cli);
}

void pcap_on_finalize(struct eds_client *cli) {
  struct pcap_client_common *ccli = PCAP_CLIENT_COMMON(cli);

  if (ccli->flags & FLAGS_HASMSGBUF) {
    ycl_msg_cleanup(&ccli->msgbuf);
    ccli->flags &= ~FLAGS_HASMSGBUF;
  }
}

