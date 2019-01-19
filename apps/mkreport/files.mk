mkreport_DEPS     = lib/ycl/ycl.c lib/ycl/ycl_msg.c \
                    lib/net/tcpproto_types.c lib/util/netstring.c \
                    lib/util/buf.c lib/util/io.c
mkreport_DEPSOBJS = ${mkreport_DEPS:.c=.o}
mkreport_SOURCES  = apps/mkreport/services.c apps/mkreport/main.c
mkreport_HEADERS  =
mkreport_OBJS     = ${mkreport_SOURCES:.c=.o}
mkreport_BIN      = apps/mkreport/mkreport

OBJS += $(mkreport_OBJS)
BINS += $(mkreport_BIN)
