#ifndef NETSTRING_H_
#define NETSTRING_H_

#include <stddef.h>

#define NS_OK             0
#define NS_ERRFMT        -1
#define NS_ERRTOOLARGE   -2
#define NS_ERRINCOMPLETE -3

const char *netstring_strerror(int code);
int netstring_parse(char **out, size_t *outlen, char *src, size_t srclen);

#endif /* NETSTRING_H_ */
