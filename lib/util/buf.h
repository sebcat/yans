#ifndef BUF_H_
#define BUF_H_

#include <stddef.h>

/* a user may read the fields in here, but 'data' may change if
 * buf_achar or buf_adata is called, so it's important to not keep
 * pointers into 'data' efter calls to buf_achar or buf_adata
 * */
typedef struct buf_t {
	size_t cap, len;
	char *data;
} buf_t;

#define buf_clear(buf) ((buf)->len = 0)

buf_t *buf_init(buf_t *buf, size_t cap);
void buf_cleanup(buf_t *buf);
int buf_grow(buf_t *buf, size_t needed);
int buf_achar(buf_t *buf, int ch);
int buf_adata(buf_t *buf, const void *data, size_t len);
void buf_shrink(buf_t *buf, size_t nbytes);

#endif
