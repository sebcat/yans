/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>

#include <lib/util/macros.h>
#include <apps/webfetch/modules/logger.h>

#define DEFAULT_OUTPATH "-"

struct logger_data {
  FILE *out;
};

int logger_init(struct module_data *mod) {
  int ch;
  int ret;
  const char *outpath = DEFAULT_OUTPATH;
  char *optstr = "o:";
  struct logger_data *data;
  struct option opts[] = {
    {"out-logfile", required_argument, NULL, 'o'},
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

  data = calloc(1, sizeof(struct logger_data));
  if (!data) {
    fprintf(stderr, "failed to allocate logger data\n");
    return -1;
  }

  ret = opener_fopen(mod->opener, outpath, "wb", &data->out);
  if (ret < 0) {
    fprintf(stderr, "%s: %s\n", outpath, opener_strerror(mod->opener));
    free(data);
    return -1;
  }

  mod->mod_data = data;
  return 0;
usage:
  fprintf(stderr, "usage: logger [--out-logfile <path>]\n");
  return -1;
}

void logger_cleanup(void *data) {
  struct logger_data *d;

  if (data) {
    d = data;
    if (d->out) {
      fclose(d->out);
      d->out = NULL;
    }
    free(d);
  }
}

void logger_process(struct fetch_transfer *t, void *data) {
  char *crlf;
  char *start;
  size_t len;
  struct logger_data *sd;
  char status_line[128];
  char timebuf[128];
  time_t tval = 0;
  struct tm tmval = {0};

  sd = data;

  /* get the time and format the time prefix */
  timebuf[0] = '\0'; /* paranoia: if strftime fails */
  time(&tval);
  localtime_r(&tval, &tmval);
  strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S %z", &tmval);

  /* copy the status line from the headers */
  status_line[0] = '\0';
  len = MIN(fetch_transfer_headerlen(t), sizeof(status_line));
  if (len > 0) {
    start = fetch_transfer_header(t);
    crlf = memmem(start, len, "\r\n", 2);
    if (crlf) {
      len = crlf - start;
      strncpy(status_line, fetch_transfer_header(t), len);
      status_line[len] = '\0';
    }
  }

  fprintf(sd->out,
      "[%s] addr:%s url:%s headerlen:%zu bodylen:%zu status-line:%s\n",
      timebuf,
      fetch_transfer_dstaddr(t),
      fetch_transfer_url(t),
      fetch_transfer_headerlen(t),
      fetch_transfer_bodylen(t),
      status_line);
}
