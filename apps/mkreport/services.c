#include <stdlib.h>
#include <stdio.h>

#include <lib/net/tcpproto.h>
#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>
#include <apps/mkreport/services.h>

static void convert(struct ycl_ctx *ycl, struct ycl_msg *msg, FILE *out,
    FILE *in) {
  struct ycl_msg_banner banner;
  int ret;

  while (ycl_readmsg(ycl, msg, in) == YCL_OK) {
    ret = ycl_msg_parse_banner(msg, &banner);
    if (ret != YCL_OK) {
      fprintf(stderr, "failed to parse banner\n");
      break;
    }

    /* FIXME: Encode CSV */
    fprintf(out, "%s,%ld,%s,%s,%zu\n", banner.host.data, banner.port,
        banner.name.data,
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
