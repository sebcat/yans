#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <lib/ycl/ycl.h>
#include <lib/ycl/ycl_msg.h>

#define DEFAULT_SOCK "/var/ethd/ethframe.sock"

struct ethframe_opts {
  const char *sock;
  struct ycl_msg_ethframe_req req;
};

static int parse_frame(int index, char *frame, struct ycl_data *out) {
  size_t strl;
  size_t i;

  strl = strlen(frame);
  if ((strl & 1) != 0) {
    fprintf(stderr, "frame %d hex-length is not divisable by two\n",
        index);
    return -1;
  } else if (strl < 28) {
    fprintf(stderr, "frame %d is too small to be an ethernet frame\n",
        index);
    return -1;
  }

  /* TODO: I don't remember hex decoding looking this ugly. Also, maybe
   *       move to lib/util/hex.[ch] */
  for (i = 0; i < strl; i += 2) {
    /* A-F -> a-f */
    if (frame[i] >= 'A' && frame[i] <= 'F') {
      frame[i] += 'a'-'A';
    }
    if (frame[i+1] >= 'A' && frame[i+1] <= 'F') {
      frame[i+1] += 'a'-'A';
    }

    if (frame[i] >= '0' && frame[i] <= '9') {
      frame[i>>1] = (frame[i] - '0') << 4;
    } else if (frame[i] >= 'a' && frame[i] <= 'f') {
      frame[i>>1] = (frame[i] - 'a' + 10) << 4;
    } else {
      fprintf(stderr, "frame %d offset %zu: invalid character\n",
          index, i);
      return -1;
    }

    if (frame[i+1] >= '0' && frame[i+1] <= '9') {
      frame[i>>1] |= (frame[i+1] - '0') & 0x0f;
    } else if (frame[i+1] >= 'a' && frame[i+1] <= 'f') {
      frame[i>>1] |= (frame[i+1] - 'a' + 10) & 0x0f;
    } else {
      fprintf(stderr, "frame %d offset %zu: invalid character\n",
          index, i+1);
      return -1;
    }
  }

  out->data = frame;
  out->len = strl / 2;
  return 0;
}

static int parse_frames(struct ethframe_opts *opts, int nframes,
    char *frames[]) {
  size_t i;

  if (nframes <= 0) {
    return 0;
  }

  opts->req.ncustom_frames = (size_t)nframes;
  opts->req.custom_frames = calloc(nframes, sizeof(struct ycl_data));
  if (opts->req.custom_frames == NULL) {
    fprintf(stderr, "frames: %s\n", strerror(errno));
    return -1;
  }

  opts->req.ncustom_frames = (size_t)nframes;
  for (i = 0; i < nframes; i++) {
    if (parse_frame(i, frames[i], &opts->req.custom_frames[i]) < 0) {
      return -1;
    }
  }

  return 0;
}

static int parse_opts(struct ethframe_opts *opts, int argc, char *argv[]) {
  int ch;
  static const struct option ps[] = {
    {"help", no_argument, NULL, 'h'},
    {"categories", required_argument, NULL, 'c'},
    {"iface", required_argument, NULL, 'i'},
    {"pps", required_argument, NULL, 'r'},
    {"src-eth", required_argument, NULL, 's'},
    {"dst-eth", required_argument, NULL, 'd'},
    {"src-ip", required_argument, NULL, 'S'},
    {"dst-ips", required_argument, NULL, 'D'},
    {"dst-ports", required_argument, NULL, 'p'},
    {"socket", required_argument, NULL, 'x'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc, argv, "c:i:r:s:d:S:D:p:x:h", ps,
      NULL)) != -1) {
    switch(ch) {
    case 'c':
      opts->req.categories = optarg;
      break;
    case 'i':
      opts->req.iface = optarg;
      break;
    case 'r':
      opts->req.pps = strtol(optarg, NULL, 10);
      break;
    case 's':
      opts->req.eth_src = optarg;
      break;
    case 'd':
      opts->req.eth_dst = optarg;
      break;
    case 'S':
      opts->req.ip_src = optarg;
      break;
    case 'D':
      opts->req.ip_dsts = optarg;
      break;
    case 'p':
      opts->req.port_dsts = optarg;
      break;
    case 'x':
      opts->sock = optarg;
      break;
    case 'h':
    default:
      goto usage;
    }
  }

  if (opts->sock == NULL) {
    opts->sock = DEFAULT_SOCK;
  }

  if (parse_frames(opts, argc - optind, argv + optind) < 0) {
    return -1;
  }

  return 0;
usage:
  fprintf(stderr, "usage: <flags> [<frame0> ... [frameN]]\n"
    "flags:\n"
    "  -h|--help                        - this text\n"
    "  -c|--categories <categories>     - sender categories to use\n"
    "  -i|--iface <iface>               - interface to send on\n"
    "  -r|--pps <n>                     - packet per second rate\n"
    "  -s|--src-eth <xx:xx:xx:xx:xx:xx> - ethernet source address\n"
    "  -d|--dst-eth <xx:xx:xx:xx:xx:xx> - ethernet destination address\n"
    "  -S|--src-ip <ip-addr>            - IP source address\n"
    "  -D|--dst-ips <ip-addrs>          - IP destination address\n"
    "  -p|--dst-ports <ports>           - TCP/UDP destination ports\n"
    "  -x|--socket <path>               - ethframe socket\n"
    "frames must be hex encoded\n");
  return -1;
}

static int ethframecli_run(struct ethframe_opts *opts) {
  struct ycl_ctx ycl;
  struct ycl_msg msg = {{0}};
  struct ycl_msg_status_resp resp = {0};
  int ret = -1;

  if (ycl_connect(&ycl, opts->sock) != YCL_OK) {
    fprintf(stderr, "%s\n", ycl_strerror(&ycl));
    return -1;
  }

  ycl_msg_init(&msg);

  if (ycl_msg_create_ethframe_req(&msg, &opts->req) != YCL_OK) {
    fprintf(stderr, "unable to create pcap request\n");
    goto done;
  }

  if (ycl_sendmsg(&ycl, &msg) < 0) {
    fprintf(stderr, "%s\n", ycl_strerror(&ycl));
    goto done;
  }

  ycl_msg_reset(&msg);
  if (ycl_recvmsg(&ycl, &msg) != YCL_OK) {
    fprintf(stderr, "%s\n", ycl_strerror(&ycl));
    goto done;
  }

  if (ycl_msg_parse_status_resp(&msg, &resp) != YCL_OK) {
    fprintf(stderr, "unable to parse pcap status response\n");
    goto done;
  }

  if (resp.errmsg != NULL) {
    fprintf(stderr, "%s\n", resp.errmsg);
    goto done;
  }

  ret = 0;
done:
  ycl_msg_cleanup(&msg);
  ycl_close(&ycl);
  return ret;
}

int ethframecli_main(int argc, char *argv[]) {
  struct ethframe_opts opts = {0};
  int ret = EXIT_FAILURE;

  if (parse_opts(&opts, argc, argv) < 0) {
    goto done;
  }

  if (ethframecli_run(&opts) < 0) {
    goto done;
  }

  ret = EXIT_SUCCESS;
done:
  return ret;
}
