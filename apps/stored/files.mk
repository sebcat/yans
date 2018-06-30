stored_DEPS = \
    lib/util/io.c \
    lib/util/buf.c \
    lib/util/netstring.c \
    lib/util/nullfd.c \
    lib/util/os.c \
    lib/util/ylog.c \
    lib/util/eds.c \
    lib/util/prng.c \
    lib/util/sindex.c \
    lib/ycl/ycl.c \
    lib/ycl/ycl_msg.c

stored_DEPSOBJS = ${stored_DEPS:.c=.o}

stored_SOURCES = \
    apps/stored/store.c \
    apps/stored/main.c

stored_HEADERS = \
    apps/stored/store.h

stored_OBJS = ${stored_SOURCES:.c=.o}

stored_BIN = apps/stored/stored

stored_MANPAGES1 = apps/stored/stored.1

OBJS += ${stored_OBJS}
BINS += ${stored_BIN}
MANPAGES1 += $(stored_MANPAGES1)
