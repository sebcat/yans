/* fio.h - C FILE* I/O utility functions */
#ifndef FIO_H_
#define FIO_H_

#include <stdio.h>

#define FIO_OK           0
#define FIO_ERR         -1 /* errno error */
#define FIO_ERRTOOLARGE -2
#define FIO_ERRFMT      -3
#define FIO_ERRUNEXPEOF -4

const char *fio_strerror(int err);

/* read a netstring to 'buf'. fails if netstring >= len. Always
 * \0-terminated. Returns FIO_OK on success or an error code on
 * failure */
int fio_readns(FILE *fp, char *buf, size_t len);

/* read a netstring of 'maxsz'-1 bytes to a malloc-ed buffer.
 * returns FIO_OK on success or an error code on failure */
int fio_readnsa(FILE *fp, size_t maxsz, char **out, size_t *outlen);

int fio_writens(FILE *fp, const char *data, size_t len);

#endif
