#ifndef PROTO_PROTO_H__
#define PROTO_PROTO_H__

#include <lib/util/netstring.h>

#define proto_strerror(code__) \
    netstring_strerror(code__)

#define PROTO_OK NETSTRING_OK
#define PROTO_ERRINCOMPLETE NETSTRING_ERRINCOMPLETE


#endif /* PROTO_PROTO_H__ */
