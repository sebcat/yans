sc2_DEPS = \
    lib/util/io.c \
    lib/util/os.c \
    lib/util/buf.c \
    lib/util/eds.c \
    lib/util/ylog.c \
    lib/util/sandbox.c

sc2_DEPSOBJS = ${sc2_DEPS:.c=.o}

sc2_SOURCES = \
    apps/sc2/main.c

sc2_HEADERS =

sc2_OBJS = ${sc2_SOURCES:.c=.o}

sc2_BIN = apps/sc2/sc2

OBJS += $(sc2_OBJS)
nodist_BINS += $(sc2_BIN)
