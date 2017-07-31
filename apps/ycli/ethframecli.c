#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <lib/ycl/ycl.h>

#define DEFAULT_SOCK "/var/ethd/ethframe.sock"

struct ethframe_opts {
  struct ycl_ethframe_req req;
  const char *sock;
};

static void clean_opts(struct ethframe_opts *opts) {
  if (opts->req.frames != NULL) {
    free(opts->req.frames);
  }

  if (opts->req.frameslen != NULL) {
    free(opts->req.frameslen);
  }
}

static int parse_frame(int index, char *frame, size_t *len) {
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

  *len = strl / 2;
  return 0;
}

static int parse_frames(struct ethframe_opts *opts, int nframes,
    char *frames[]) {
  int i;

  if (nframes <= 0) {
    return 0;
  }

  opts->req.frames = calloc(opts->req.nframes, sizeof(char*));
  if (opts->req.frames == NULL) {
    fprintf(stderr, "frames: %s\n", strerror(errno));
    return -1;
  }

  opts->req.frameslen = calloc(opts->req.nframes, sizeof(size_t));
  if (opts->req.frameslen == NULL) {
    fprintf(stderr, "frameslen: %s\n", strerror(errno));
    free(opts->req.frames);
    return -1;
  }

  opts->req.nframes = (size_t)nframes;
  for (i = 0; i < nframes; i++) {
    if (parse_frame(i, frames[i], &opts->req.frameslen[i]) < 0) {
      return -1;
    }
    opts->req.frames[i] = frames[i];
  }

  return 0;
}

static int parse_opts(struct ethframe_opts *opts, int argc, char *argv[]) {
  int ch;
  int ret;
  struct in_addr addr;
  static unsigned char hwa[6];
  static const struct option ps[] = {
    {"iface", required_argument, NULL, 'i'},
    {"sock", required_argument, NULL, 's'},
    {"arpreqs", required_argument, NULL, 'a'},
    {"spa", required_argument, NULL, 'p'},
    {"sha", required_argument, NULL, 'e'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
  };

  while ((ch = getopt_long(argc, argv, "i:s:a:p:e:h", ps, NULL)) != -1) {
    switch(ch) {
    case 'i':
      opts->req.iface = optarg;
      break;
    case 's':
      opts->sock = optarg;
      break;
    case 'a':
      opts->req.arpreq_addrs = optarg;
      break;
    case 'p':
      if (!inet_aton(optarg, &addr)) {
        fprintf(stderr, "invalid SPA\n");
        goto usage;
      }
      opts->req.arpreq_spa = addr.s_addr;
      break;
    case 'e':
      ret = sscanf(optarg, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &hwa[0], &hwa[1],
          &hwa[2], &hwa[3], &hwa[4], &hwa[5]);
      if (ret != 6) {
        fprintf(stderr, "unable to parse ethernet address: \"%s\"", optarg);
        goto usage;
      }
      opts->req.arpreq_sha = (char*)hwa;
      break;
    case 'h':
    default:
      goto usage;
    }
  }

  if (opts->req.iface == NULL) {
    fprintf(stderr, "no iface set\n");
    goto usage;
  }

  if (opts->sock == NULL) {
    opts->sock = DEFAULT_SOCK;
  }

  if (parse_frames(opts, argc - optind, argv + optind) < 0) {
    clean_opts(opts);
    return -1;
  }

  return 0;
usage:
  fprintf(stderr, "usage: <flags> <frame0> ... <frameN>\n"
    "flags:\n"
    "  -s|--sock <sock>         - path to ethframe socket\n"
    "  -i|--iface <iface>       - sender interface (required)\n"
    "  -a|--arpreqs <addrspec>  - addrspec for ARP requests\n"
    "  -p|--spa <ip4 addr>      - ARP SPA\n"
    "  -e|--sha <eth addr>      - ARP SHA\n"
    "frames must be hex encoded\n");
  return -1;
}

static int ethframecli_run(struct ethframe_opts *opts) {
  struct ycl_ctx ycl;
  struct ycl_msg msg = {0};
  const char *okmsg = NULL;
  const char *errmsg = NULL;
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

  if (ycl_msg_parse_status_resp(&msg, &okmsg, &errmsg) != YCL_OK) {
    fprintf(stderr, "unable to parse pcap status response\n");
    goto done;
  }

  if (errmsg != NULL) {
    fprintf(stderr, "%s\n", errmsg);
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
  clean_opts(&opts);
  return ret;
}
