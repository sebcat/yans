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
#include <string.h>
#include <assert.h>

#include <lib/net/fcgi.h>

#define INIT_MBUFSZ 2048
#define INIT_PBUFSZ 2048
#define INIT_BBUFSZ 2048

#define MAX_PARAMSZ (1 << 20) * 2
#define MAX_BODYSZ  (1 << 20) * 10 /* large data should be streamed */

/* record reading states */
#define RSTATE_INCOMPLETE 1
#define RSTATE_COMPLETE   2

/* param reading states */
#define PSTATE_EXPECT_BEGIN_REQUEST 1
#define PSTATE_EXPECT_PARAMS        2
#define PSTATE_DONE                 3

#define FCGIERR_CLOSED          -1
#define FCGIERR_UNEXPECTEDMSG   -2
#define FCGIERR_INVALIDSTATE    -3
#define FCGIERR_MALFORMED       -4
#define FCGIERR_PARAMSIZE       -5
#define FCGIERR_BODYSIZE        -6

int fcgi_cli_init(struct fcgi_cli *fcgi, int fd) {
  if (buf_init(&fcgi->buf, INIT_MBUFSZ) == NULL) {
    return -1;
  }

  if (buf_init(&fcgi->params, INIT_PBUFSZ) == NULL) {
    buf_cleanup(&fcgi->buf);
    return -1;
  }

  /* FIXME: this should be on-demand, if the caller chooses to buffer the
   *        request body (zero the buf here, allocate if zeroed in
   *        fcgi_cli_readbody) */
  if (buf_init(&fcgi->body, INIT_BBUFSZ) == NULL) {
    buf_cleanup(&fcgi->buf);
    buf_cleanup(&fcgi->params);
    return -1;
  }

  IO_INIT(&fcgi->io, fd);
  fcgi->id = 0;
  fcgi->read_state = RSTATE_INCOMPLETE;
  fcgi->param_state = PSTATE_EXPECT_BEGIN_REQUEST;
  fcgi->errcode = 0;
  return 0;
}

void fcgi_cli_cleanup(struct fcgi_cli *fcgi) {
  buf_cleanup(&fcgi->buf);
  buf_cleanup(&fcgi->params);
  buf_cleanup(&fcgi->body);
  /* don't close the fd here, it should be managed externally (e.g., by eds) */
}

static int fcgi_is_complete_msg(struct fcgi_cli *fcgi) {
  struct fcgi_header *hdr;
  size_t msglen;

  if (fcgi->buf.len < sizeof(struct fcgi_header)) {
    return 0;
  }

  hdr = (struct fcgi_header *)fcgi->buf.data;
  msglen = sizeof(struct fcgi_header) + FCGI_CLEN(hdr) + hdr->plen;
  if (msglen > fcgi->buf.len) {
    return 0;
  }

  return 1;
}

int fcgi_cli_readmsg(struct fcgi_cli *fcgi, struct fcgi_msg *msg) {
  struct fcgi_header *hdr;
  size_t len;
  size_t nlen;
  size_t nread;
  int ret;
  int is_complete;

again:
  if (fcgi->read_state == RSTATE_COMPLETE) {
    assert(fcgi->buf.len >= sizeof(struct fcgi_header));
    hdr = (struct fcgi_header *)fcgi->buf.data;
    len = sizeof(struct fcgi_header) + FCGI_CLEN(hdr) + hdr->plen;
    if (len < fcgi->buf.len) {
      /* we have trailing bytes (perhaps a full message), move them to the
       * beginning of the buffer and update the buffer length */
      nlen = fcgi->buf.len - len;
      memmove(fcgi->buf.data, fcgi->buf.data + len, nlen);
      fcgi->buf.len = nlen;

      if (fcgi_is_complete_msg(fcgi)) {
        /* we had a full message in the trailing bytes in the buffer */
        goto got_msg;
      }
    } else {
      /* we don't have trailing bytes, reset the buffer */
      buf_clear(&fcgi->buf);
    }

    fcgi->read_state = RSTATE_INCOMPLETE;
  }

  do {
    ret = io_readbuf(&fcgi->io, &fcgi->buf, &nread);
    is_complete = fcgi_is_complete_msg(fcgi);
  } while (!is_complete && ret == IO_OK && nread > 0);

  if (is_complete) {
    fcgi->read_state = RSTATE_COMPLETE;
    goto got_msg;
  } else if (ret == IO_AGAIN) {
    return FCGI_AGAIN;
  } else if (ret == IO_OK && nread == 0) {
    fcgi->errcode = FCGIERR_CLOSED;
    return FCGI_ERR;
  }

  return FCGI_ERR;

got_msg:
  msg->header = (struct fcgi_header *)fcgi->buf.data;
  msg->data = fcgi->buf.data + sizeof(struct fcgi_header);
  if (fcgi->param_state != PSTATE_EXPECT_BEGIN_REQUEST &&
      msg->header->id != fcgi->id) {
    /* unexpected ID, ignore message and try and get another one */
    goto again;
  }

  return FCGI_OK;
}

int fcgi_cli_readparams(struct fcgi_cli *fcgi) {
  struct fcgi_msg msg;
  size_t clen;
  int ret;

  if (fcgi->param_state == PSTATE_DONE) {
    return FCGI_OK;
  }

  while ((ret = fcgi_cli_readmsg(fcgi, &msg)) == FCGI_OK) {
    clen = FCGI_CLEN(msg.header);
    switch(fcgi->param_state) {
    case PSTATE_EXPECT_BEGIN_REQUEST:
      if (msg.header->t != FCGI_BEGIN_REQUEST) {
        fcgi->errcode = FCGIERR_UNEXPECTEDMSG;
        return FCGI_ERR;
      }
      fcgi->id = msg.header->id;
      fcgi->param_state = PSTATE_EXPECT_PARAMS;
      break;
    case PSTATE_EXPECT_PARAMS:
      if (msg.header->t != FCGI_PARAMS) {
        fcgi->errcode = FCGIERR_UNEXPECTEDMSG;
        return FCGI_ERR;
      }
      if (clen == 0) {
        fcgi->param_state = PSTATE_DONE;
        return FCGI_OK;
      }
      if (fcgi->params.len + clen > MAX_PARAMSZ) {
        fcgi->errcode = FCGIERR_PARAMSIZE;
        return FCGI_ERR;
      }
      buf_adata(&fcgi->params, msg.data, clen);
      break;
    default:
      fcgi->errcode = FCGIERR_INVALIDSTATE;
      return FCGI_ERR;
    }
  }
  return ret;
}

int fcgi_cli_readbody(struct fcgi_cli *fcgi) {
  size_t clen;
  struct fcgi_msg msg;
  int ret;

  while ((ret = fcgi_cli_readmsg(fcgi, &msg)) == FCGI_OK) {
    clen = FCGI_CLEN(msg.header);
    if (msg.header->t != FCGI_STDIN) {
      fcgi->errcode = FCGIERR_UNEXPECTEDMSG;
      return FCGI_ERR;
    }
    if (clen == 0) {
      return FCGI_OK;
    } else if (fcgi->body.len + clen > MAX_BODYSZ) {
      fcgi->errcode = FCGIERR_BODYSIZE;
      return FCGI_ERR;
    }
    buf_adata(&fcgi->body, msg.data, clen);
  }

  return ret;
}

int fcgi_cli_readreq(struct fcgi_cli *fcgi) {
  int ret;

  if (fcgi->param_state == PSTATE_DONE) {
    return fcgi_cli_readbody(fcgi);
  }

  if ((ret = fcgi_cli_readparams(fcgi)) == FCGI_OK) {
    ret = fcgi_cli_readbody(fcgi);
  }
  return ret;
}

static int get_length(const char **data, size_t *len, size_t *out) {
  const char *ptr = *data;
  size_t left = *len;

  if (left < 1) {
    return -1;
  }

  if (*ptr & 0x80) {
    if (left < 4) {
      return -1;
    }
    *out = (size_t)ptr[0] << 24 | (size_t)ptr[1] << 16 |
        (size_t)ptr[2] << 8 | (size_t)ptr[3];
    *data += 4;
    *len -= 4;
    return 4;
  } else {
    *out = *ptr;
    *data += 1;
    *len -= 1;
    return 1;
  }
}

/* FCGI_OK on done, FCGI_AGAIN on more, FCGI_ERR on failure */
int fcgi_cli_next_param(struct fcgi_cli *fcgi, size_t *off,
    struct fcgi_pair *out) {
  size_t left;
  const char *ptr;
  size_t keylen;
  size_t valuelen;
  int l0;
  int l1;

  if (*off >= fcgi->params.len) {
    return FCGI_OK;
  }

  left = fcgi->params.len - *off;
  ptr = fcgi->params.data + *off;

  if ((l0 = get_length(&ptr, &left, &keylen)) < 0) {
    return FCGI_OK;
  }

  if ((l1 = get_length(&ptr, &left, &valuelen)) < 0) {
    return FCGI_OK;
  }

  if (keylen + valuelen > left) {
    return FCGI_OK;
  }

  out->key = ptr;
  out->keylen = keylen;
  out->value = ptr + keylen;
  out->valuelen = valuelen;

  *off += l0 + l1 + keylen + valuelen;
  return FCGI_AGAIN;
}

void fcgi_cli_format_header(struct fcgi_cli *cli, struct fcgi_header *h,
    uint8_t t, unsigned short clen) {
  h->version = 1;
  h->t = t;
  h->id = cli->id; /* we assume id is given in big-endian form */
  h->clenB1 = (uint8_t)(clen >> 8);
  h->clenB0 = (uint8_t)(clen & 0xff);
  h->plen = 0;
}


void fcgi_cli_format_endmsg(struct fcgi_cli *cli, struct fcgi_end_request *r,
    int status) {
  fcgi_cli_format_header(cli, &r->header, FCGI_END_REQUEST, 8);
  r->appstatusB3 = (uint8_t)(status >> 24);
  r->appstatusB2 = (uint8_t)(status >> 16);
  r->appstatusB1 = (uint8_t)(status >> 8);
  r->appstatusB0 = (uint8_t)(status & 0xff);
  r->pstatus = FCGI_REQUEST_COMPLETE;
}

const char *fcgi_cli_strerror(struct fcgi_cli *fcgi) {
  if (fcgi->errcode == 0) {
    return io_strerror(&fcgi->io);
  } else {
    switch(fcgi->errcode) {
    case FCGIERR_CLOSED:
      return "premature connection termination";
    case FCGIERR_UNEXPECTEDMSG:
      return "an unexpected message was received";
    case FCGIERR_INVALIDSTATE:
      return "invalid internal state";
    case FCGIERR_MALFORMED:
      return "malformed message";
    case FCGIERR_PARAMSIZE:
      return "FCGI_PARAMS buffer limit exceeded";
    case FCGIERR_BODYSIZE:
      return "FCGI_STDIN buffer limit exceeded";
    default:
      return "unknown error";
    }
  }
}

const char *fcgi_type_to_str(int t) {
  switch(t) {
  case FCGI_BEGIN_REQUEST:
    return "FCGI_BEGIN_REQUEST";
  case FCGI_ABORT_REQUEST:
    return "FCGI_ABORT_REQUEST";
  case FCGI_END_REQUEST:
    return "FCGI_END_REQUEST";
  case FCGI_PARAMS:
    return "FCGI_PARAMS";
  case FCGI_STDIN:
    return "FCGI_STDIN";
  case FCGI_STDOUT:
    return "FCGI_STDOUT";
  case FCGI_STDERR:
    return "FCGI_STDERR";
  case FCGI_DATA:
    return "FCGI_DATA";
  case FCGI_GET_VALUES:
    return "FCGI_GET_VALUES";
  case FCGI_GET_VALUES_RESULT:
    return "FCGI_GET_VALUES_RESULT";
  case FCGI_UNKNOWN_TYPE:
    return "FCGI_UNKNOWN_TYPE";
  default:
    return "FCGI_???";
  }
}
