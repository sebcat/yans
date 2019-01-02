#ifndef UTIL_LINES_H__
#define UTIL_LINES_H__

#include <stdio.h>

/* 'lines' is just a line buffering abstraction with the following ideas:
 *
 * - A chunk of lines is read at a time (lines_next_chunk)
 * - A line in a chunk is valid until the next chunk is read
 * - For the lifetime of a chunk, there's no need to copy a single line
 * - Lines are mutable
 */

/* option flags */
#define LINES_FCOMPR (1 << 0) /* decompress data from 'fd' */

/* status codes */
#define LINES_CONTINUE     1
#define LINES_OK           0
#define LINES_EOPEN       -1
#define LINES_ENOMEM      -2
#define LINES_EREAD       -3

#define LINES_IS_ERR(x) (x < 0)

struct lines_ctx {
  /* internal */
  FILE *fp;
  size_t cap;     /* size of 'data' (excluding trailing \0 byte) */
  size_t len;     /* length of read actual data in 'data' */
  size_t end;     /* end of last complete line in 'data' */
  size_t lineoff; /* current line offset */
  char *data;     /* chunk of file */
};

int lines_init(struct lines_ctx *ctx, int fd, int flags);
void lines_cleanup(struct lines_ctx *ctx);
int lines_next_chunk(struct lines_ctx *ctx);
int lines_next(struct lines_ctx *ctx, char **out, size_t *outlen);
const char *lines_strerror(int err);

#endif
