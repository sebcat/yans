a2_DEPS     = lib/util/buf.c lib/util/netstring.c lib/util/io.c \
              lib/net/scgi.c
a2_DEPSOBJS = ${a2_DEPS:.c=.o}
a2_SOURCES  = apps/a2/yapi.c apps/a2/main.c
a2_HEADERS  = apps/a2/yapi.h
a2_OBJS     = ${a2_SOURCES:.c=.o}
a2_SHLIB    = apps/a2/a2.so

OBJS += $(a2_OBJS)
nodist_SHLIBS += $(a2_SHLIB)
