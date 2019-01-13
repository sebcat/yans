sysinfoapi_DEPS = \
    lib/util/io.c \
    lib/util/os.c \
    lib/util/buf.c \
    lib/util/eds.c \
    lib/util/ylog.c \
    lib/util/sysinfo.c

sysinfoapi_DEPSOBJS = ${sysinfoapi_DEPS:.c=.o}

sysinfoapi_SOURCES = \
    apps/sysinfoapi/sysinfoapi.c \
    apps/sysinfoapi/main.c

sysinfoapi_HEADERS =

sysinfoapi_OBJS = ${sysinfoapi_SOURCES:.c=.o}

sysinfoapi_BIN = apps/sysinfoapi/sysinfoapi

OBJS += $(sysinfoapi_OBJS)
BINS += $(sysinfoapi_BIN)
