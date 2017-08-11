#ifndef NET_FCGI_H__
#define NET_FCGI_H__

#include <lib/util/io.h>
#include <lib/util/buf.h>

#include <stdint.h>

/* FCGI types */
#define FCGI_BEGIN_REQUEST       1
#define FCGI_ABORT_REQUEST       2
#define FCGI_END_REQUEST         3
#define FCGI_PARAMS              4
#define FCGI_STDIN               5
#define FCGI_STDOUT              6
#define FCGI_STDERR              7
#define FCGI_DATA                8
#define FCGI_GET_VALUES          9
#define FCGI_GET_VALUES_RESULT   10
#define FCGI_UNKNOWN_TYPE        11

struct fcgi_header {
  uint8_t version;
  uint8_t t;
  uint16_t id;
  uint8_t clenB1;
  uint8_t clenB0;
  uint8_t plen;
  uint8_t reserved;
} __attribute__((packed));

/* non-multiplexing (because of FCGI protocol limitations, and because
 * no one implements it) FCGI responder */
struct fcgi_cli {
  /* -- internal fields -- */
  uint16_t id;
  buf_t buf;        /* message buffer */
  buf_t params;     /* FCGI_PARAMS */
  buf_t body;       /* FCGI_STDIN */
  int read_state;   /* state of record reading */
  int param_state;  /* state of param reading */
  int errcode;
  io_t io;
};

struct fcgi_msg {
  struct fcgi_header *header;
  const char *data;
};

struct fcgi_pair {
  size_t keylen;
  size_t valuelen;
  const char *key;
  const char *value;
};

#define FCGI_REQUEST_COMPLETE 0
struct fcgi_end_request {
  struct fcgi_header header;
  uint8_t appstatusB3;
  uint8_t appstatusB2;
  uint8_t appstatusB1;
  uint8_t appstatusB0;
  uint8_t pstatus;
  uint8_t res[3];
} __attribute__((packed));

#define FCGI_ENDMSGSIZE 16

#define FCGI_CLEN(hdr) \
    (((hdr)->clenB1 << 8) + (hdr)->clenB0)

#define FCGI_ERR  -1
#define FCGI_OK    0
#define FCGI_AGAIN 1

int fcgi_cli_init(struct fcgi_cli *fcgi, int fd);
void fcgi_cli_cleanup(struct fcgi_cli *fcgi);

/* returns FCGI_OK on success, setting the fields of msg to point at the
 * message in the internal message buffer, which is valid until the next
 * call to fcgi_cli_readmsg. Returns FCGI_AGAIN in non-blocking mode to
 * signal that reading should be resumed when more data becomes available.
 * Returns FCGI_ERR on error (please use fcgi_cli_strerror). */
int fcgi_cli_readmsg(struct fcgi_cli *fcgi, struct fcgi_msg *msg);

/* reads FCGI_BEGIN_REQUEST and FCGI_PARAMS and returns FCGI_OK when
 * all params are read, or FCGI_AGAIN in non-blocking mode to signal that
 * reading should be resumed when more data becomes available. Returns
 * FCGI_ERR on error. */
int fcgi_cli_readparams(struct fcgi_cli *fcgi);
int fcgi_cli_readbody(struct fcgi_cli *fcgi);
int fcgi_cli_readreq(struct fcgi_cli *fcgi);

int fcgi_cli_next_param(struct fcgi_cli *fcgi, size_t *off,
    struct fcgi_pair *out);

void fcgi_cli_format_endmsg(struct fcgi_cli *cli, struct fcgi_end_request *r,
    int status);
void fcgi_cli_format_header(struct fcgi_cli *cli, struct fcgi_header *h,
    uint8_t t, unsigned short clen);


const char *fcgi_cli_strerror(struct fcgi_cli *fcgi);
const char *fcgi_type_to_str(int t);



#endif /* NET_FCGI_H__ */
