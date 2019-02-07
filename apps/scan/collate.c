#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <lib/util/csv.h>
#include <lib/net/tcpproto_types.h>
#include <apps/scan/collate.h>

#define MAX_INOUTS 8
#define MAX_FOPENS 128

enum collate_type {
  COLLATE_UNKNOWN = 0,
  COLLATE_BANNERS,
  COLLATE_MAX,
};

struct collate_opts {
  enum collate_type type;

  FILE *in_banners[MAX_INOUTS];
  size_t nin_banners;

  FILE *out_services_csv[MAX_INOUTS];
  size_t nout_services_csv;

  /* TODO: out_certs... */

  FILE *closefps[MAX_FOPENS];
  size_t nclosefps;
};

static int banners(struct scan_ctx *ctx, struct collate_opts *opts) {
  size_t ibindex = 0;
  size_t osvcsindex = 0;
  struct ycl_ctx ycl;
  struct ycl_msg msg;
  int status = EXIT_FAILURE;
  int ret;
  struct ycl_msg_banner banner;
  buf_t buf;
  struct sockaddr *sa;
  socklen_t salen;
  const char *fields[5];
  char addrbuf[128];
  char portbuf[8];

  ycl_init(&ycl, YCL_NOFD);
  ycl_msg_init(&msg);
  if (!buf_init(&buf, 128 * 1024)) {
    goto end;
  }

  for (ibindex = 0; ibindex < opts->nin_banners; ibindex++) {
    while (ycl_readmsg(&ycl, &msg, opts->in_banners[ibindex]) == YCL_OK) {
      ret = ycl_msg_parse_banner(&msg, &banner);
      if (ret != YCL_OK) {
        goto end;
      }

      sa = (struct sockaddr *)banner.addr.data;
      salen = (socklen_t)banner.addr.len;
      ret = getnameinfo(sa, salen, addrbuf, sizeof(addrbuf),
          portbuf, sizeof(portbuf), NI_NUMERICHOST | NI_NUMERICSERV);
      if (ret != 0) {
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(ret));
        continue;
      }

      buf_clear(&buf);
      fields[0] = banner.name.data;
      fields[1] = addrbuf;
      fields[2] = "tcp";
      fields[3] = portbuf;
      fields[4] = tcpproto_type_to_string((enum tcpproto_type)banner.mpid);
      ret = csv_encode(&buf, fields, sizeof(fields) / sizeof(*fields));
      if (ret != 0) {
        fprintf(stderr, "csv_encode failure\n");
        break; /* memory error - don't bother continuing */
      }

      for (osvcsindex = 0;
          osvcsindex < opts->nout_services_csv;
          osvcsindex++) {
        fwrite(buf.data, buf.len, 1, opts->out_services_csv[osvcsindex]);
      }
    }
  }

  status = EXIT_SUCCESS;
end:
  ycl_msg_cleanup(&msg);
  ycl_close(&ycl);
  return status;
}

enum collate_type collate_type_from_str(const char *str) {
  static const struct {
    char *name;
    enum collate_type type;
  } map[] = {
    {"banners", COLLATE_BANNERS},
  };
  size_t i;

  for (i = 0; i < sizeof(map) / sizeof(*map); i++) {
    if (strcmp(map[i].name, str) == 0) {
      return map[i].type;
    }
  }

  return COLLATE_UNKNOWN;
}

static int open_io(struct collate_opts *opts, struct opener_ctx *opener,
    const char *path, const char *mode, FILE **dst, size_t *ndst) {

  FILE *fp;
  int ret;

  if (*ndst >= MAX_INOUTS || opts->nclosefps >= MAX_FOPENS) {
    fprintf(stderr, "too many inputs\n");
    return -1;
  }

  ret = opener_fopen(opener, path, mode, &fp);
  if (ret < 0) {
    fprintf(stderr, "%s: %s\n", optarg, opener_strerr(opener));
    return -1;
  }

  dst[*ndst] = fp;
  opts->closefps[opts->nclosefps++] = fp;
  *ndst = *ndst + 1;
  return 0;
}

typedef int (*collate_func_t)(struct scan_ctx *, struct collate_opts *);

static void usage(const char *argv0) {
  fprintf(stderr, "usage: %s collate <opts>\n"
      "opts:\n"
      "  -t|--type <name>\n"
      "      Collation type\n"
      "  -B|--in-banners <path>\n"
      "      Banner input\n"
      "  -s|--out-services-csv <path>\n"
      "      Services CSV output\n"
      "  -X|--no-sandbox\n"
      "      Disable sandbox\n"
      ,argv0);
}

int collate_main(struct scan_ctx *scan, int argc, char **argv) {
  const char *tmpstr;
  const char *argv0;
  const char *optstr = "t:B:s:X";
  static struct collate_opts opts;
  collate_func_t func;
  collate_func_t funcs[COLLATE_MAX] = {
    [COLLATE_BANNERS]  = banners,
  };
  /* short opts: uppercase letters for inputs, if sane to do so */
  static const struct option lopts[] = {
    {"type",             required_argument, NULL, 't'},
    {"in-banners",       required_argument, NULL, 'B'},
    {"out-services-csv", required_argument, NULL, 's'},
    {"no-sandbox",       no_argument,       NULL, 'X'},
    {NULL, 0, NULL, 0}};
  int ch;
  int status = EXIT_FAILURE;
  int ret;
  int i;
  int sandbox = 1;

  argv0 = argv[0];
  argc--;
  argv++;

  while ((ch = getopt_long(argc, argv, optstr, lopts, NULL)) != -1) {
    switch (ch) {
    case 't': /* type */
      opts.type = collate_type_from_str(optarg);
      break;
    case 'B': /* in-banners */
      ret = open_io(&opts, &scan->opener, optarg, "rb",
          opts.in_banners, &opts.nin_banners);
      if (ret < 0) {
        goto end;
      }
      break;
    case 's': /* out-services-csv */
      ret = open_io(&opts, &scan->opener, optarg, "wb",
          opts.out_services_csv, &opts.nout_services_csv);
      if (ret < 0) {
        goto end;
      }

      tmpstr = "Name,Address,Transport,Port,Service\r\n";
      fwrite(tmpstr, 1, strlen(tmpstr),
          opts.out_services_csv[opts.nout_services_csv-1]);
      break;
    case 'X':
      sandbox = 0;
      break;
    default:
      usage(argv0);
      goto end;
    }
  }

  if (opts.type <= COLLATE_UNKNOWN || opts.type >= COLLATE_MAX) {
    fprintf(stderr, "missing or invalid collation\n");
    usage(argv0);
    goto end;
  }

  func = funcs[opts.type];
  if (func == NULL) {
    fprintf(stderr, "collation %d has no callback\n", opts.type);
    goto end;
  }

  if (sandbox) {
    ret = sandbox_enter();
    if (ret != 0) {
      fprintf(stderr, "failed to enter sandbox mode\n");
      goto end;
    }
  } else {
    fprintf(stderr, "warning: sandbox disabled\n");
  }

  status = func(scan, &opts);
end:
  for (i = 0; i < opts.nclosefps; i++) {
    fclose(opts.closefps[i]);
  }

  return status;
}
