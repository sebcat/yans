#include <stdlib.h>
#include <string.h>
#include <lib/util/buf.h>

/* BUF_ALIGNMENT must be a power of two */
#define BUF_ALIGNMENT    sizeof(long)
#define BUF_ALIGN(x)    (((x)+(BUF_ALIGNMENT-1))&~(BUF_ALIGNMENT-1))


buf_t *buf_init(buf_t *buf, size_t cap) {
  size_t acap = BUF_ALIGN(cap);
  if ((buf->data = calloc(1, acap)) == NULL) {
    return NULL;
  }

  buf->cap = acap;
  buf->len = 0;
  return buf;
}

void buf_cleanup(buf_t *buf) {
  if (buf->data != NULL) {
    free(buf->data);
    buf->data = NULL;
  }
}

int buf_grow(buf_t *buf, size_t needed) {
  size_t added;
  char *ndata;

  added = BUF_ALIGN(buf->cap/2);
  if (added < needed) {
    added = BUF_ALIGN(needed);
  }

  if ((ndata = realloc(buf->data, buf->cap + added)) == NULL) {
    return -1;
  }

  buf->cap += added;
  buf->data = ndata;
  return 0;
}

int buf_achar(buf_t *buf, int ch) {
  if (buf->cap == buf->len && buf_grow(buf, 1) < 0) {
    return -1;
  }

  buf->data[buf->len++] = (char)ch;
  return 0;
}

int buf_adata(buf_t *buf, const void *data, size_t len) {
  size_t nleft = buf->cap - buf->len;

  if (nleft < len && buf_grow(buf, len-nleft) < 0) {
    return -1;
  }

  memcpy(buf->data + buf->len, data, len);
  buf->len += len;
  return 0;
}

void buf_shrink(buf_t *buf, size_t nbytes) {
  if (nbytes > buf->len) {
    buf->len = 0;
  } else {
    buf->len -= nbytes;
  }
}
