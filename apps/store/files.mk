store_DEPS = \
    lib/util/netstring.c \
    lib/util/buf.c \
    lib/util/io.c \
    lib/ycl/ycl_msg.c \
    lib/ycl/ycl.c \

store_DEPSOBJS = ${store_DEPS:.c=.o}

store_SOURCES = \
    apps/store/main.c

store_HEADERS =

store_OBJS = ${store_SOURCES:.c=.o}

store_BIN = apps/store/store

OBJS += $(store_OBJS)
BINS += $(store_BIN)
