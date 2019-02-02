expand_dst_DEPS = \
    lib/util/buf.c \
    lib/util/reorder.c \
    lib/net/ip.c \
    lib/net/ports.c \
    lib/net/dsts.c \

expand_dst_DEPSOBJS = ${expand_dst_DEPS:.c=.o}

expand_dst_SOURCES = \
    apps/expand-dst/main.c

expand_dst_HEADERS =

expand_dst_OBJS = ${expand_dst_SOURCES:.c=.o}

expand_dst_BIN = apps/expand-dst/expand-dst

OBJS += $(expand_dst_OBJS)
nodist_BINS += $(expand_dst_BIN)
