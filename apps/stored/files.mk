stored_DEPS = \
    lib/util/io.c \
    lib/util/buf.c \
    lib/util/netstring.c \
    lib/util/os.c \
    lib/util/ylog.c \
    lib/util/eds.c \
    lib/util/prng.c \
    lib/ycl/ycl.c \
    lib/ycl/ycl_msg.c

stored_DEPSOBJS = ${stored_DEPS:.c=.o}

stored_SOURCES = \
    apps/stored/nullfd.c \
    apps/stored/store.c \
    apps/stored/main.c

stored_HEADERS = \
    apps/stored/nullfd.h \
    apps/stored/store.h

stored_OBJS = ${stored_SOURCES:.c=.o}

stored_BIN = apps/stored/stored

OBJS += ${stored_OBJS}
BINS += ${stored_BIN}
