#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>

#include <lib/util/io.h>
#include <lib/util/ylog.h>
#include <lib/util/netstring.h>

#include <proto/status_resp.h>

#include <apps/ethd/pcap.h>

#define CMDBUFINITSZ 1024     /* initial size of allocated cmd buffer */
#define MAX_CMDSZ (1 << 20)   /* maximum size of cmd request */
#define SNAPLEN 2048          /* pcap_open_live snapshot length */
#define PCAP_TO_MS 1000       /* pcap_open_live timeout, in ms */
#define PCAP_DISPATCH_CNT 64  /* pcap_dispatch dispatch count */

#define PCAP_CLIENT(cli__) \
  (struct pcap_client *)((cli__)->udata)

static void on_terminate(struct eds_client *cli, int fd) {
  eds_client_clear_actions(cli);
}

enum resptype {
  RESPTYPE_OK,
  RESPTYPE_ERR,
};

static void sendresp(struct eds_client *cli, enum resptype t,
    const char *msg) {
  struct p_status_resp resp = {0};
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
    resp.okmsglen = strlen(msg);
  } else {
    eds_client_set_on_readable(cli, NULL); /* stop any reader routines */
    trans.flags = EDS_TFLREAD | EDS_TFLWRITE;
    trans.on_readable = NULL;
    trans.on_writable = NULL;
    resp.errmsg = msg;
    resp.errmsglen = strlen(msg);
  }

  /* serialize the response */
  buf_clear(&pcapcli->cmdbuf); /* reuse cmdbuf for responses */
  ret = p_status_resp_serialize(&resp, &pcapcli->cmdbuf);
  if (ret != PROTO_OK) {
    ylog_error("pcapcli%d: senderr serialization error: %s",
        eds_client_get_fd(cli), proto_strerror(ret));
    eds_client_clear_actions(cli);
    return;
  }

  eds_client_send(cli, pcapcli->cmdbuf.data, pcapcli->cmdbuf.len, &trans);
}

static void on_gotpkg(struct eds_client *cli, int fd) {
  struct eds_client *parentcli;
  struct pcap_client *pcapcli;

  memcpy(&parentcli, cli->udata, sizeof(struct eds_client *));
  pcapcli = PCAP_CLIENT(parentcli);
  if (pcap_dispatch(pcapcli->pcap, PCAP_DISPATCH_CNT, pcap_dump,
      (u_char*)pcapcli->dumper) < 0) {
    ylog_error("pcapcli%d: pcap_dispatch: %s", eds_client_get_fd(parentcli),
        pcap_geterr(pcapcli->pcap));
    eds_client_clear_actions(parentcli);
    return;
  }
}

static void on_readcmd(struct eds_client *cli, int fd) {
  struct pcap_client *pcapcli = PCAP_CLIENT(cli);
  struct eds_client *dumpcli;
  struct eds_client_actions acts = {0};
  char errbuf[PCAP_ERRBUF_SIZE];
  struct bpf_program bpf = {0};
  int ret;
  int pcapfd;
  io_t io;
  size_t left;
  size_t nread;
  struct p_pcap_req cmd;
  const char *errmsg = "an internal error occurred";

  IO_INIT(&io, fd);

  ret = io_readbuf(&io, &pcapcli->cmdbuf, &nread);
  if (ret == IO_AGAIN) {
    return;
  } else if (ret != IO_OK) {
    ylog_error("pcapcli%d: io_readbuf: %s", fd, io_strerror(&io));
    goto fail;
  }

  if (nread == 0) {
    ylog_error("pcapcli%d: connection terminated while reading cmd", fd);
    goto fail;
  }

  if (pcapcli->cmdbuf.len >= MAX_CMDSZ) {
    ylog_error("pcapcli%d: maximum command size exceeded", fd);
    errmsg = "request too large";
    goto fail;
  }

  ret = p_pcap_req_deserialize(&cmd, pcapcli->cmdbuf.data,
      pcapcli->cmdbuf.len, &left);
  if (ret == PROTO_ERRINCOMPLETE) {
    return;
  } else if (ret != PROTO_OK) {
    ylog_error("pcapcli%d: p_pcapd_cmd_deserialize: %s", fd,
        proto_strerror(ret));
    goto fail;
  }

  if (left != 0) {
    /* we have trailing data. Since the only thing we expect more than the
     * initial request is a termination message, we're done */
    eds_client_clear_actions(cli);
    return;
  }

  if (cmd.iface == NULL) {
    ylog_error("pcapcli%d: iface not set", fd);
    errmsg = "no interface specified";
    goto fail;
  }

  ylog_info("pcapcli%d: iface:\"%s\" %s filter", fd, cmd.iface,
      cmd.filter == NULL ? "without" : "with");

  errbuf[0] = '\0';
  if ((pcapcli->pcap = pcap_open_live(cmd.iface, SNAPLEN, 0,
      PCAP_TO_MS, errbuf)) == NULL) {
    ylog_error("pcapcli%d: pcap_open_live: %s", fd, errbuf);
    snprintf(pcapcli->msg, sizeof(pcapcli->msg), "%s", errbuf);
    errmsg = pcapcli->msg;
    goto fail;
  } else if (errbuf[0] != '\0') {
    ylog_info("pcapcli%d: pcap_open_live warning: %s", fd, errbuf);
  }

  if (cmd.filter != NULL) {
    if (pcap_compile(pcapcli->pcap, &bpf, cmd.filter, 1,
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
  if ((dumpcli = eds_service_add_client(cli->svc, pcapfd, &acts, &cli,
      sizeof(cli))) == NULL) {
    ylog_error("pcapcli%d: eds_service_add_client failure", fd);
    goto fail;
  }

  eds_client_set_externalfd(dumpcli);
  pcapcli->dumpcli = dumpcli;

  /* send OK response and terminate the client whenever we get data */
  sendresp(cli, RESPTYPE_OK, "started");
  eds_client_set_on_readable(cli, on_terminate);
  return;

fail:
  eds_client_clear_actions(cli);
  sendresp(cli, RESPTYPE_ERR, errmsg);
}

/* initial on_readable event */
void pcap_on_readable(struct eds_client *cli, int fd) {
  struct pcap_client *pcapcli = PCAP_CLIENT(cli);
  io_t io;
  int pcapfd;
  FILE *fp;

  IO_INIT(&io, fd);
  if (io_recvfd(&io, &pcapfd) != IO_OK) {
    ylog_error("pcapcli%d: io_recvfd: %s", fd, io_strerror(&io));
    eds_client_clear_actions(cli);
    return;
  }

  fp = fdopen(pcapfd, "w");
  if (fp == NULL) {
    ylog_error("pcapcli%d: fdopen: %s", fd, strerror(errno));
    close(pcapfd);
    eds_client_clear_actions(cli);
    return;
  }

  pcapcli->dumpf = fp;
  buf_init(&pcapcli->cmdbuf, CMDBUFINITSZ);
  eds_client_set_on_readable(cli, on_readcmd);
  on_readcmd(cli, fd);
}

void pcap_on_done(struct eds_client *cli, int fd) {
  struct pcap_client *pcapcli = PCAP_CLIENT(cli);

  ylog_info("pcapcli%d: done", fd);
  buf_cleanup(&pcapcli->cmdbuf);
  if (pcapcli->dumpf != NULL) {
    fclose(pcapcli->dumpf);
  }
  if (pcapcli->pcap != NULL) {
    pcap_close(pcapcli->pcap);
  }
  if (pcapcli->dumper != NULL) {
    pcap_dump_close(pcapcli->dumper);
  }
  if (pcapcli->dumpcli != NULL) {
    eds_service_remove_client(cli->svc, pcapcli->dumpcli);
  }
}
