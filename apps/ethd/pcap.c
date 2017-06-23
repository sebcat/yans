#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>

#include <lib/util/io.h>
#include <lib/util/ylog.h>
#include <lib/util/netstring.h>

#include <apps/ethd/pcap.h>

#define CMDBUFINITSZ 1024     /* initial size of allocated cmd buffer */
#define MAX_CMDSZ (1 << 20)   /* maximum size of cmd request */
#define SNAPLEN 2048          /* pcap_open_live snapshot length */
#define PCAP_TO_MS 1000       /* pcap_open_live timeout, in ms */
#define PCAP_DISPATCH_CNT 64  /* pcap_dispatch dispatch count */

#define PCAP_CLIENT(cli__) \
  (struct pcap_client *)((cli__)->udata)

static eds_action_result on_gotpkg(struct eds_client *cli, int fd) {
  struct eds_client *parentcli;
  struct pcap_client *pcapcli;
  int cmdfd = eds_client_get_fd(cli);

  /* copy the parent client with memcpy to prevent strict aliasing breakage */
  memcpy(&parentcli, cli->udata, sizeof(struct eds_client *));
  pcapcli = PCAP_CLIENT(parentcli);

  if (pcap_dispatch(pcapcli->pcap, PCAP_DISPATCH_CNT, pcap_dump,
      (u_char*)pcapcli->dumper) < 0) {
    ylog_error("pcapcli%d: pcap_dispatch: %s", cmdfd,
        pcap_geterr(pcapcli->pcap));

    /* remove the parent client from the pool; this will also close the
     * pcap handle and it's file descriptor */
    eds_service_remove_client(cli->svc, parentcli);
    return EDS_DONE;
  }

  return EDS_CONTINUE;
}

static eds_action_result on_terminate(struct eds_client *cli, int fd) {
  return EDS_DONE;
}

static eds_action_result on_readcmd(struct eds_client *cli, int fd) {
  struct pcap_client *pcapcli = PCAP_CLIENT(cli);
  struct eds_client *dumpcli;
  struct eds_client_actions acts = {0};
  char errbuf[PCAP_ERRBUF_SIZE];
  struct bpf_program bpf = {0};
  int ret;
  int pcapfd;
  io_t io;

  IO_INIT(&io, fd);

  if (io_readbuf(&io, &pcapcli->cmdbuf, NULL) != IO_OK) {
    ylog_error("pcapcli%d: io_readbuf: %s", fd, io_strerror(&io));
    goto fail;
  }

  if (pcapcli->cmdbuf.len >= MAX_CMDSZ) {
    ylog_error("pcapcli%d: maximum command size exceeded", fd);
    goto fail;
  }

  ret = p_pcap_cmd_deserialize(&pcapcli->cmd, pcapcli->cmdbuf.data,
      pcapcli->cmdbuf.len);
  if (ret == PROTO_ERRINCOMPLETE) {
    return EDS_CONTINUE;
  } else if (ret != PROTO_OK) {
    ylog_error("pcapcli%d: p_pcapd_cmd_deserialize: %s", fd,
        proto_strerror(ret));
    goto fail;
  }

  if (pcapcli->cmd.iface == NULL) {
    ylog_error("pcapcli%d: iface not set", fd);
    goto fail;
  }

  ylog_info("pcapcli%d: iface:\"%s\" %s filter", fd, pcapcli->cmd.iface,
      pcapcli->cmd.filter == NULL ? "without" : "with");

  errbuf[0] = '\0';
  if ((pcapcli->pcap = pcap_open_live(pcapcli->cmd.iface, SNAPLEN, 0,
      PCAP_TO_MS, errbuf)) == NULL) {
    ylog_error("pcapcli%d: pcap_open_live: %s", fd, errbuf);
    goto fail;
  } else if (errbuf[0] != '\0') {
    ylog_info("pcapcli%d: pcap_open_live warning: %s", fd, errbuf);
  }

  if (pcapcli->cmd.filter != NULL) {
    if (pcap_compile(pcapcli->pcap, &bpf, pcapcli->cmd.filter, 1,
        PCAP_NETMASK_UNKNOWN) < 0) {
      ylog_error("pcapcli%d: pcap_compile: %s", fd,
          pcap_geterr(pcapcli->pcap));
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

  pcapcli->dumpf = NULL; /* cli->dumper has ownership over cli->dumpf, and will
                          * clear it on pcap_dump_close */
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

  /* close the client when it becomes readable again (hopefully when the
   * remote end closes it's connection) */
  eds_client_set_on_readable(cli, on_terminate);
  return EDS_CONTINUE;

fail:
  return EDS_DONE;
}

/* initial on_readable event */
eds_action_result pcap_on_readable(struct eds_client *cli, int fd) {
  struct pcap_client *pcapcli = PCAP_CLIENT(cli);
  io_t io;
  int pcapfd;
  FILE *fp;

  IO_INIT(&io, fd);
  if (io_recvfd(&io, &pcapfd) != IO_OK) {
    ylog_error("pcapcli%d: io_recvfd: %s", fd, io_strerror(&io));
    return EDS_DONE;
  }

  fp = fdopen(pcapfd, "w");
  if (fp == NULL) {
    ylog_error("pcapcli%d: fdopen: %s", fd, strerror(errno));
    close(pcapfd);
    return EDS_DONE;
  }

  pcapcli->dumpf = fp;
  buf_init(&pcapcli->cmdbuf, CMDBUFINITSZ);
  eds_client_set_on_readable(cli, on_readcmd);
  return EDS_CONTINUE;
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
