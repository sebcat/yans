knegd_DEPS = \
    lib/util/io.c \
    lib/util/netstring.c \
    lib/util/os.c \
    lib/util/buf.c \
    lib/util/eds.c \
    lib/util/str.c \
    lib/util/ylog.c \
    lib/ycl/ycl.c \
    lib/ycl/ycl_msg.c

knegd_DEPSOBJS = ${knegd_DEPS:.c=.o}

knegd_SOURCES = \
    apps/knegd/kng.c \
    apps/knegd/main.c

knegd_HEADERS = \
    apps/knegd/kng.h

knegd_OBJS = ${knegd_SOURCES:.c=.o}

knegd_BIN = apps/knegd/knegd

OBJS += $(knegd_OBJS)
BINS += $(knegd_BIN)
