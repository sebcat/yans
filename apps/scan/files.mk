scan_DEPS = \
    lib/util/netstring.c \
    lib/util/buf.c \
    lib/util/io.c \
    lib/ycl/ycl.c \
    lib/ycl/ycl_msg.c

scan_DEPSOBJS = ${scan_DEPS:.c=.o}

scan_SOURCES = \
    apps/scan/main.c

scan_HEADERS =

scan_OBJS = ${scan_SOURCES:.c=.o}

scan_BIN = apps/scan/scan

OBJS += $(scan_OBJS)
BINS += $(scan_BIN)
