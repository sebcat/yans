#ifndef NETSTRING_H_
#define NETSTRING_H_

#include <stddef.h>

#include <lib/util/buf.h>

#define NETSTRING_OK             0
#define NETSTRING_ERRFMT        -1
#define NETSTRING_ERRTOOLARGE   -2
#define NETSTRING_ERRINCOMPLETE -3
#define NETSTRING_ERRMEM        -4

struct netstring_pair {
	char *key;
	size_t keylen;
	char *value;
	size_t valuelen;
};

const char *netstring_strerror(int code);
int netstring_parse(char **out, size_t *outlen, char *src, size_t srclen);
int netstring_append_buf(buf_t *buf, const char *str, size_t len);
int netstring_next_pair(struct netstring_pair *res, char **data,
    size_t *datalen);

#endif /* NETSTRING_H_ */
