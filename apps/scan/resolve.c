#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>

#include <lib/util/buf.h>
#include <lib/util/sandbox.h>
#include <lib/ycl/resolvercli.h>
#include <apps/scan/opener.h>
#include <apps/scan/resolve.h>

#define DFL_INPUTFILE  "-"
#define DFL_OUTPUTFILE "-"

#define NAME_LIMITSZ (50 * 1024 * 1024)

static int read_names(struct opener_ctx *opener, buf_t *buf,
    const char *infile) {
  size_t navail;
  size_t nread;
  FILE *fp;
  int ret;
  int status = -1;

  ret = opener_fopen(opener, infile, "rb", &fp);
  if (ret < 0) {
    fprintf(stderr, "opener_fopen input: %s\n",
        opener_strerr(opener));
    return -1;
  }

  for (;;) {
    /* if there's less than 512 bytes left, grow 50% or 512 bytes, whichever
     * is larger */
    if (buf->len >= NAME_LIMITSZ) {
      fprintf(stderr, "exceeded total name max size: %d bytes\n",
          NAME_LIMITSZ);
      goto end;
    }

    /* Grow buffer, if needed */
    navail = buf->cap - buf->len;
    if (navail < 512) {
      if (buf_grow(buf, 512) < 0) {
        perror("read_names");
        goto end;
      }
    }

    nread = fread(buf->data + buf->len, 1, navail, fp);
    if (nread == 0) {
      break;
    }

    buf->len += nread;
  }

  if (ferror(fp)) {
    perror("fread");
    goto end;
  }

  status = 0;
end:
  fclose(fp);
  return status;
}

int resolve_main(struct scan_ctx *scan, int argc, char **argv) {
  const char *optstr = "i:o:s:X";
  static const struct option lopts[] = {
    {"input",      required_argument, NULL, 'i'},
    {"output",     required_argument, NULL, 'o'},
    {"socket",     required_argument, NULL, 's'},
    {"no-sandbox", no_argument,       NULL, 'X'},
    {NULL, 0 , NULL, 0},
  };
  const char *infile = DFL_INPUTFILE;
  const char *outfile = DFL_OUTPUTFILE;
  const char *socket = RESOLVERCLI_DFLPATH;
  const char *argv0 = argv[0];
  int status = EXIT_FAILURE;
  int ch;
  int ret;
  buf_t buf;
  int use_zlib = 0;
  int outfd = -1;
  int sandbox = 1;
  struct ycl_ctx ycl;
  struct resolvercli_ctx resolvercli;

  argv++;
  argc--;
  while ((ch = getopt_long(argc, argv, optstr, lopts, NULL)) != -1) {
    switch (ch) {
    case 'i':
      infile = optarg;
      break;
    case 'o':
      outfile = optarg;
      break;
    case 's':
      socket = optarg;
      break;
    case 'X':
      sandbox = 0;
      break;
    default:
      goto usage;
    }
  }

  ret = ycl_connect(&ycl, socket);
  if (ret != YCL_OK) {
    fprintf(stderr, "ycl_connect: %s\n", ycl_strerror(&ycl));
    goto end;
  }

  if (sandbox) {
    ret = sandbox_enter();
    if (ret != 0) {
      perror("sandbox_enter");
    }
  }

  if (!buf_init(&buf, 16 * 1024)) {
    goto end_ycl_close;
  }

  ret = read_names(&scan->opener, &buf, infile);
  if (ret < 0) {
    goto end_buf_cleanup;
  }

  ret = opener_open(&scan->opener, outfile, O_WRONLY | O_CREAT,
    &use_zlib, &outfd);
  if (ret < 0) {
    fprintf(stderr, "opener_fopen input: %s\n",
        opener_strerr(&scan->opener));
    goto end_buf_cleanup;
  }

  resolvercli_init(&resolvercli, &ycl, &scan->msgbuf);
  ret = resolvercli_resolve(&resolvercli, outfd, buf.data, buf.len,
    use_zlib);
  if (ret != RESOLVERCLI_OK) {
    fprintf(stderr, "resolvercli_resolve: %s\n",
        resolvercli_strerror(&resolvercli));
    close(outfd);
    goto end_buf_cleanup;
  }

  close(outfd);
  status = EXIT_SUCCESS;
end_buf_cleanup:
  buf_cleanup(&buf);
end_ycl_close:
  ycl_close(&ycl);
end:
  return status;
usage:
  fprintf(stderr, "%s %s [--input <in>] [--output <out>]\n", argv0,
      argv[1]);
  return EXIT_FAILURE;
}
