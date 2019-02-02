scan_DEPS     = lib/net/dsts.c lib/net/ip.c lib/net/ports.c \
                lib/net/reaplan.c lib/net/tcpproto_types.c \
                lib/net/tcpproto.c lib/net/tcpsrc.c lib/util/buf.c \
                lib/util/idset.c lib/util/io.c lib/util/lines.c \
                lib/util/netstring.c lib/util/reorder.c lib/util/sandbox.c \
                lib/util/str.c lib/util/zfile.c lib/ycl/resolvercli.c \
                lib/ycl/storecli.c lib/ycl/ycl_msg.c lib/ycl/ycl.c
scan_DEPS_CC  = lib/util/reset.cc
scan_DEPSOBJS = ${scan_DEPS:.c=.o} ${scan_DEPS_CC:.cc=.o}
scan_SOURCES  = apps/scan/main.c apps/scan/opener.c apps/scan/resolve.c \
                apps/scan/banners.c apps/scan/bgrab.c apps/scan/collate.c
scan_HEADERS  = apps/scan/opener.h apps/scan/resolve.h apps/scan/banners.h \
                apps/scan/bgrab.h apps/scan/collate.h
scan_OBJS     = ${scan_SOURCES:.c=.o}
scan_BIN      = apps/scan/scan
scan_LDADD   != pkg-config --libs re2
scan_LDADD   += -lz -lssl -lcrypto -lstdc++

OBJS += $(scan_OBJS)
BINS += $(scan_BIN)
