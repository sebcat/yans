ethd_DEPS = \
    lib/util/buf.c \
    lib/util/io.c \
    lib/util/os.c \
    lib/util/netstring.c \
    lib/util/eds.c \
    lib/util/ylog.c \
    lib/util/zfile.c \
    lib/util/flagset.c \
    lib/ycl/ycl.c \
    lib/ycl/ycl_msg.c \
    lib/net/ip.c \
    lib/net/iface.c \
    lib/net/eth.c \
    lib/net/ports.c

ethd_DEPSOBJS = ${ethd_DEPS:.c=.o}

ethd_SOURCES = \
    apps/ethd/ethframe.c \
    apps/ethd/pcap.c \
    apps/ethd/main.c

ethd_HEADERS = \
    apps/ethd/ethframe.h \
    apps/ethd/pcap.h

ethd_OBJS = ${ethd_SOURCES:.c=.o}

ethd_LDADD_Linux = -lcap
ethd_LDADD := -lz -lpcap ${ethd_LDADD_${UNAME_S}}

ethd_BIN = apps/ethd/ethd

OBJS += ${ethd_OBJS}
BINS += ${ethd_BIN}
