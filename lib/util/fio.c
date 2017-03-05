#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <lib/util/fio.h>

const char *fio_strerror(int err) {
  switch(err) {
    case FIO_ERR:
      return strerror(errno);
    case FIO_ERRTOOLARGE:
      return "fio: too large message";
    case FIO_ERRFMT:
      return "fio: invalid message format";
    case FIO_ERRUNEXPEOF:
      return "fio: unexpeced end of message";
    case FIO_OK:
      return "fio: success";
    default:
      return "fio: unknown error";
  }
}

int fio_readns(FILE *fp, char *buf, size_t len) {
  unsigned int msglen = 0;
  if (fscanf(fp, "%u:", &msglen) != 1) {
    return FIO_ERRFMT;
  }
  if (msglen >= len)  {
    return FIO_ERRTOOLARGE;
  }
  if (fread(buf, 1, msglen, fp) != msglen) {
    return FIO_ERRUNEXPEOF;
  }
  buf[msglen] = '\0';
  if (fgetc(fp) != ',') {
    return FIO_ERRFMT;
  }
  return FIO_OK;
}

int fio_readnsa(FILE *fp, size_t maxsz, char **out, size_t *outlen) {
  unsigned int msglen = 0;
  char *buf;
  if (fscanf(fp, "%u:", &msglen) != 1) {
    return FIO_ERRFMT;
  }
  if (msglen >= maxsz)  {
    return FIO_ERRTOOLARGE;
  }
  if ((buf = malloc((size_t)msglen+1)) == NULL) {
    return FIO_ERR;
  }
  if (fread(buf, 1, msglen, fp) != msglen) {
    return FIO_ERRUNEXPEOF;
  }
  buf[msglen] = '\0';
  if (fgetc(fp) != ',') {
    return FIO_ERRFMT;
  }
  if (out != NULL) {
    *out = buf;
  } else {
    free(buf);
  }
  if (outlen != NULL) {
    *outlen = (size_t)msglen;
  }
  return FIO_OK;
}

int fio_writens(FILE *fp, const char *data, size_t len) {
  unsigned int msglen = (unsigned int)len;
  errno = 0;
  if (fprintf(fp, "%u:", msglen) < 0) {
    return FIO_ERR;
  }
  if (fwrite(data, 1, len, fp) < len) {
    return FIO_ERR;
  }
  if (fputc(',', fp) != ',') {
    return FIO_ERR;
  }
  if (fflush(fp) != 0) {
    return FIO_ERR;
  }
  return FIO_OK;
}
