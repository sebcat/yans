clid_DEPS = \
    lib/util/buf.c \
    lib/util/eds.c \
    lib/util/io.c \
    lib/util/netstring.c \
    lib/util/os.c \
    lib/util/u8.c \
    lib/util/ylog.c \
    lib/util/zfile.c \
    lib/ycl/ycl.c \
    lib/ycl/ycl_msg.c \
    lib/net/dnstres.c \
    lib/net/punycode.c
clid_DEPSOBJS = ${clid_DEPS:.c=.o}
clid_SOURCES = apps/clid/resolver.c apps/clid/main.c
clid_HEADERS = apps/clid/resolver.h

clid_OBJS = ${clid_SOURCES:.c=.o}

clid_BIN = apps/clid/clid

clid_LDADD = -lz -lpthread

OBJS += $(clid_OBJS)
BINS += $(clid_BIN)
