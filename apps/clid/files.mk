clid_DEPS = \
    lib/util/io.c \
    lib/util/os.c \
    lib/util/buf.c \
    lib/util/eds.c \
    lib/util/ylog.c \
    lib/util/netstring.c \
    lib/ycl/ycl.c \
    lib/ycl/ycl_msg.c \
    lib/net/iface.c \
    lib/net/route.c

clid_DEPSOBJS = ${clid_DEPS:.c=.o}

clid_SOURCES = \
    apps/clid/routes.c \
    apps/clid/main.c

clid_HEADERS = \
    apps/clid/routes.h

clid_OBJS = ${clid_SOURCES:.c=.o}

clid_BIN = apps/clid/clid

OBJS += $(clid_OBJS)
BINS += $(clid_BIN)
