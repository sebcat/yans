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
#define NETSTRING_MENTRY(__type, __member) \
  {.key = #__member, .offset = offsetof(__type, __member)}

/* macro for defining the end of a netstring_map table */
#define NETSTRING_MENTRY_END {0}

/*

  NETSTRING_MENTRY and NETSTRING_MENTRY_END should be used as follows:

  struct mystruct {
    char *a_val;
    char *b_val;
    char *c_val;
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
  size_t offset;
};

const char *netstring_strerror(int code);
int netstring_parse(char **out, size_t *outlen, char *src, size_t srclen);
int netstring_append_buf(buf_t *buf, const char *str, size_t len);
int netstring_append_pair(buf_t *buf, const char *str0, size_t len0,
    const char *str1, size_t len1);
int netstring_next_pair(struct netstring_pair *res, char **data,
    size_t *datalen);

int netstring_serialize(void *data, struct netstring_map *map, buf_t *out);
int netstring_deserialize(void *data, struct netstring_map *map, char *in,
    size_t inlen);


#endif /* NETSTRING_H_ */
