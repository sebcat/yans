#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <apps/webfetch/modules/writer.h>

#define DEFAULT_OUTPATH "-"

struct writer_data {
  FILE *out;
  struct ycl_msg msgbuf;
};

int writer_init(struct module_data *mod) {
  int ch;
  int ret;
  const char *outpath = DEFAULT_OUTPATH;
  char *optstr = "o:";
  struct writer_data *data;
  struct option opts[] = {
    {"out-httpmsgs", required_argument, NULL, 'o'},
    {NULL, 0, NULL, 0},
  };

  optind = 1; /* reset getopt(_long) state */
  while ((ch = getopt_long(mod->argc, mod->argv, optstr, opts, NULL)) != -1) {
    switch (ch) {
    case 'o':
      outpath = optarg;
      break;
    default:
      goto usage;
    }
  }

  data = calloc(1, sizeof(struct writer_data));
  if (!data) {
    fprintf(stderr, "failed to allocate writer data\n");
    goto fail;
  }

  ret = opener_fopen(mod->opener, outpath, "wb", &data->out);
  if (ret < 0) {
    fprintf(stderr, "%s: %s\n", outpath, opener_strerror(mod->opener));
    goto fail_free_data;
  }

  ret = ycl_msg_init(&data->msgbuf);
  if (ret != YCL_OK) {
    fprintf(stderr, "failed to allocate writer ycl msgbuf\n");
    goto fail_fclose;
  }

  mod->mod_data = data;
  return 0;
usage:
  fprintf(stderr, "usage: writer [--out-httpmsgs <path>]\n");
  return -1;
fail_fclose:
  fclose(data->out);
fail_free_data:
  free(data);
fail:
  return -1;
}

void writer_cleanup(void *data) {
  struct writer_data *wd;

  if (data) {
    wd = data;
    fclose(wd->out);
    ycl_msg_cleanup(&wd->msgbuf);
    free(wd);
  }
}

void writer_process(struct fetch_transfer *t, void *data) {
  int ret;
  struct ycl_msg_httpmsg msg = {{0}};
  struct writer_data *wd = data;

  msg.url.data       = fetch_transfer_url(t);
  msg.url.len        = fetch_transfer_urllen(t);
  msg.addr.data      = fetch_transfer_dstaddr(t);
  msg.addr.len       = strlen(msg.addr.data);
  msg.hostname.data  = fetch_transfer_hostname(t);
  msg.hostname.len   = strlen(msg.hostname.data);
  msg.resphdr.data   = fetch_transfer_header(t);
  msg.resphdr.len    = fetch_transfer_headerlen(t);
  msg.respbody.data  = fetch_transfer_body(t);
  msg.respbody.len   = fetch_transfer_bodylen(t);
  ret = ycl_msg_create_httpmsg(&wd->msgbuf, &msg);
  if (ret != YCL_OK) {
    fprintf(stderr, "httpmsg serialization failure in writer\n");
    return;
  }

  fwrite(ycl_msg_bytes(&wd->msgbuf), 1, ycl_msg_nbytes(&wd->msgbuf),
      wd->out);
}
