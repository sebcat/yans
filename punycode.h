#ifndef PUNYCODE_H_
#define PUNYCODE_H_
#include <stddef.h>

char *punycode_encode(const void *in, size_t len);
#endif
