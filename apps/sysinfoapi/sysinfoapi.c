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
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <lib/util/macros.h>
#include <lib/util/sysinfo.h>
#include <apps/sysinfoapi/sysinfoapi.h>

#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var"
#endif

static const char *rootpath_ = LOCALSTATEDIR;

static const char scgi_hdr_[] =
    "Status: 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "\r\n";

#define SCGI_HDRLEN (sizeof(scgi_hdr_)-1)

void sysinfoapi_set_rootpath(const char *root) {
  rootpath_ = root;
}

static void on_writebody(struct eds_client *cli, int fd) {
  struct sysinfoapi_cli *ecli = SYSINFOAPI_CLI(cli);
  ssize_t nwritten;

  do {
    nwritten = write(fd, ecli->body + ecli->offset,
        ecli->nbytes_body - ecli->offset);
    if (nwritten < 0) {
      if (errno == EAGAIN) {
        return;
      } else if (errno == EINTR) {
        continue;
      } else {
        goto end;
      }
    } else if (nwritten == 0) {
      goto end;
    }
    ecli->offset += nwritten;
  } while (ecli->offset < SCGI_HDRLEN);
end:
  eds_client_clear_actions(cli);
}

static void on_writehdr(struct eds_client *cli, int fd) {
  struct sysinfoapi_cli *ecli = SYSINFOAPI_CLI(cli);
  ssize_t nwritten;

  /* We could use writev to potentially write the header and body in one
   * call. It's easier (IMHO) to do EDS state transitions though */
  do {
    nwritten = write(fd, scgi_hdr_ + ecli->offset,
        SCGI_HDRLEN - ecli->offset);
    if (nwritten < 0) {
      if (errno == EAGAIN) {
        return;
      } else if (errno == EINTR) {
        continue;
      } else {
        goto bail;
      }
    } else if (nwritten == 0) {
      goto bail;
    }
    ecli->offset += nwritten;
  } while (ecli->offset < SCGI_HDRLEN);

  ecli->offset = 0;
  eds_client_set_on_writable(cli, on_writebody, 0);
  return;

bail:
  eds_client_clear_actions(cli);
  return;
}

void sysinfoapi_on_writable(struct eds_client *cli, int fd) {
  struct sysinfoapi_cli *ecli = SYSINFOAPI_CLI(cli);
  struct sysinfo_data sinfo;
  int ret;

  ecli->offset = 0;
  sysinfo_get(&sinfo, rootpath_);
  /* XXX: Do not add stuff here without confirming that it fits in
   *      respbuf. If you do, the JSON output will get truncated. */
  ret = snprintf(ecli->body, sizeof(ecli->body),
      "{\"fcap\":%.2f,\"icap\":%.2f,\"uptime\":%ld,"
      "\"loadavg\":[%.2f,%.2f,%.2f]}\n", 
      sinfo.fcap, sinfo.icap, sinfo.uptime, sinfo.loadavg[0],
      sinfo.loadavg[1], sinfo.loadavg[2]);
  ecli->nbytes_body = CLAMP(ret, 0, sizeof(ecli->body)-1);

  eds_client_set_on_writable(cli, on_writehdr, 0);
}
