fc2_DEPS = \
    lib/util/io.c \
    lib/util/os.c \
    lib/util/buf.c \
    lib/util/eds.c \
    lib/util/ylog.c \
    lib/net/fcgi.c

fc2_DEPSOBJS = ${fc2_DEPS:.c=.o}

fc2_SOURCES = \
    apps/fc2/fc2.c

fc2_HEADERS =

fc2_OBJS = ${fc2_SOURCES:.c=.o}

fc2_BIN = apps/fc2/fc2

OBJS += $(fc2_OBJS)
BINS += $(fc2_BIN)
