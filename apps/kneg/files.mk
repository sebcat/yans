kneg_DEPS = \
    lib/util/buf.c \
    lib/util/io.c \
    lib/util/netstring.c \
    lib/util/sandbox.c \
    lib/ycl/ycl.c \
    lib/ycl/ycl_msg.c

kneg_DEPSOBJS = ${kneg_DEPS:.c=.o}

kneg_SOURCES = \
    apps/kneg/main.c

kneg_HEADERS =

kneg_OBJS = ${kneg_SOURCES:.c=.o}

kneg_BIN = apps/kneg/kneg

OBJS += $(kneg_OBJS)
BINS += $(kneg_BIN)
