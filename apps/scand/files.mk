scand_DEPS = \
    lib/util/io.c \
    lib/util/netstring.c \
    lib/util/os.c \
    lib/util/buf.c \
    lib/util/eds.c \
    lib/util/ylog.c \
    lib/ycl/ycl.c \
    lib/ycl/ycl_msg.c

scand_DEPSOBJS = ${scand_DEPS:.c=.o}

scand_SOURCES = \
    apps/scand/scanner.c \
    apps/scand/main.c

scand_HEADERS = \
    apps/scand/scanner.h

scand_OBJS = ${scand_SOURCES:.c=.o}

scand_BIN = apps/scand/scand

OBJS += $(scand_OBJS)
BINS += $(scand_BIN)
