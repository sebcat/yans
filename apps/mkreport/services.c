#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>

#include <lib/net/tcpproto.h>
#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>
#include <apps/mkreport/services.h>

static void convert(struct ycl_ctx *ycl, struct ycl_msg *msg, FILE *out,
    FILE *in) {
  struct ycl_msg_banner banner;
  int ret;
  char addrbuf[128];
  char portbuf[8];
  struct sockaddr *sa;
  socklen_t salen;

  while (ycl_readmsg(ycl, msg, in) == YCL_OK) {
    ret = ycl_msg_parse_banner(msg, &banner);
    if (ret != YCL_OK) {
      fprintf(stderr, "failed to parse banner\n");
      break;
    }

    sa = (struct sockaddr *)banner.addr.data;
    salen = (socklen_t)banner.addr.len;
    ret = getnameinfo(sa, salen, addrbuf, sizeof(addrbuf),
        portbuf, sizeof(portbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    if (ret != 0) {
      fprintf(stderr, "getnameinfo: %s\n", gai_strerror(ret));
      continue;
    }

    /* FIXME: Encode CSV */
    fprintf(out, "%s,%s,%s,%s,%zu\n", addrbuf, portbuf, banner.name.data,
        tcpproto_type_to_string((enum tcpproto_type)banner.mpid),
        banner.banner.len);
  }
}

int services_report(struct report_opts *opts) {
  FILE *out;
  FILE *in;
  int i;
  struct ycl_ctx ycl;
  struct ycl_msg msg;

  if (opts->output) {
    out = fopen(opts->output, "wb");
    if (!out) {
      perror("fopen");
      return EXIT_FAILURE;
    }
  } else {
    out = stdout;
  }

  ycl_init(&ycl, YCL_NOFD);
  ycl_msg_init(&msg);

  for (i = 0; i < opts->ninputs; i++) {
    in = fopen(opts->inputs[i], "rb");
    if (in) {
      convert(&ycl, &msg, out, in);
      fclose(in);
    } else {
      fprintf(stderr, "Warning: unable to open %s\n", opts->inputs[i]);
    }
  }

  ycl_msg_cleanup(&msg);
  ycl_close(&ycl);

  if (out != stdout) {
    fclose(out);
  }

  return EXIT_SUCCESS;
}
