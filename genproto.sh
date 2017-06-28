#!/bin/sh

usage() {
  echo "usage: $0 <name> <fields>" >&2
  exit 1
}

if [ $# -lt 2 ]; then
  usage
fi

NAME=$1
UCNAME=$(echo "$1" | tr '[:lower:]' '[:upper:]')
shift

echo "Name: $NAME $UCNAME"
for P in "$@"; do
  echo "  $P"
done

##
## generate proto/$NAME.h
##
exec 1>"proto/$NAME.h"
cat <<EOF
#ifndef PROTO_${UCNAME}_H__
#define PROTO_${UCNAME}_H__

#include <proto/proto.h>

struct p_$NAME {
EOF

for PARAM in "$@"; do
  printf "  const char *%s;\n  size_t %slen;\n" "$PARAM" "$PARAM"
done

cat <<EOF
};

int p_${NAME}_serialize(struct p_${NAME} *data, buf_t *out);
int p_${NAME}_deserialize(struct p_${NAME} *data,
    char *in, size_t inlen, size_t *left);

#endif /* PROTO_${UCNAME}_H__ */
EOF

##
## generate proto/$NAME.c
##
exec 1>"proto/$NAME.c"

cat <<EOF
#include <string.h>

#include <proto/$NAME.h>

static struct netstring_map ${NAME}_m[] = {
EOF

for PARAM in "$@"; do
  printf "  NETSTRING_MENTRY(struct p_%s, %s),\n" "$NAME" "$PARAM"
done
printf "  NETSTRING_MENTRY_END,\n};\n"

cat <<EOF
int p_${NAME}_serialize(struct p_$NAME *data, buf_t *out) {
  return netstring_serialize(data, ${NAME}_m, out);
}

int p_${NAME}_deserialize(struct p_$NAME *data,
    char *in, size_t inlen, size_t *left) {
  memset(data, 0, sizeof(struct p_$NAME));
  return netstring_deserialize(data, ${NAME}_m, in, inlen, left);
}
EOF
