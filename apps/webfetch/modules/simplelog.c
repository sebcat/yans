#include <string.h>

#include <lib/util/macros.h>
#include <apps/webfetch/modules/simplelog.h>

void simplelog_process(struct fetch_transfer *t, void *data) {
  char *crlf;
  char *start;
  char status_line[128];
  size_t len;

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

  fprintf(stderr,
      "addr:%s url:%s headerlen:%zu bodylen:%zu status-line:%s\n",
      fetch_transfer_dstaddr(t),
      fetch_transfer_url(t),
      fetch_transfer_headerlen(t),
      fetch_transfer_bodylen(t),
      status_line);
}
