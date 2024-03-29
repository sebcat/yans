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
#ifndef NETSTRING_H_
#define NETSTRING_H_

#include <stddef.h>

#include <lib/util/buf.h>

#define NETSTRING_OK             0
#define NETSTRING_ERRFMT        -1
#define NETSTRING_ERRTOOLARGE   -2
#define NETSTRING_ERRINCOMPLETE -3
#define NETSTRING_ERRMEM        -4

/* macro for defining netstring_map entries */
#define NETSTRING_MENTRY(__type, __member)     \
  {.key = #__member,                           \
   .voff = offsetof(__type, __member),         \
   .loff = offsetof(__type, __member ## len) } \

/* macro for defining the end of a netstring_map table */
#define NETSTRING_MENTRY_END {0}

/*

  NETSTRING_MENTRY and NETSTRING_MENTRY_END should be used as follows:

  struct mystruct {
    char *a_val;
    size_t a_vallen;
    char *b_val;
    size_t b_vallen;
    char *c_val;
    size_t c_vallen;
  };

  struct netstring_map m[] = {
    NETSTRING_MENTRY(struct mystruct, a_val),
    NETSTRING_MENTRY(struct mystruct, b_val),
    NETSTRING_MENTRY(struct mystruct, c_val),
    NETSTRING_MENTRY_END,
  };
*/

/* type representing a key,value pair */
struct netstring_pair {
	char *key;
	size_t keylen;
	char *value;
	size_t valuelen;
};

/* type representing a struct serialization mapping */
struct netstring_map {
  char *key;
  size_t voff; /* value offset */
  size_t loff; /* length offset*/
};

const char *netstring_strerror(int code);
int netstring_tryparse(const char *src, size_t srclen, size_t *nextoff);
int netstring_parse(char **out, size_t *outlen, char *src, size_t srclen);
int netstring_append_buf(buf_t *buf, const char *str, size_t len);
int netstring_append_list(buf_t *buf, size_t nelems, const char **elems,
    size_t *lens);
int netstring_append_pair(buf_t *buf, const char *str0, size_t len0,
    const char *str1, size_t len1);
int netstring_next(char **out, size_t *outlen, char **data, size_t *datalen);
int netstring_next_pair(struct netstring_pair *res, char **data,
    size_t *datalen);

int netstring_serialize(void *data, struct netstring_map *map, buf_t *out);
int netstring_deserialize(void *data, struct netstring_map *map, char *in,
    size_t inlen, size_t *left);


#endif /* NETSTRING_H_ */
