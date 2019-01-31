scan_DEPS     = lib/util/buf.c lib/util/io.c lib/util/netstring.c \
                lib/util/zfile.c lib/ycl/ycl.c lib/ycl/ycl_msg.c \
                lib/ycl/storecli.c
scan_DEPSOBJS = ${scan_DEPS:.c=.o}
scan_SOURCES  = apps/scan/main.c apps/scan/opener.c apps/scan/resolve.c
scan_HEADERS  = apps/scan/opener.h apps/scan/resolve.h
scan_OBJS     = ${scan_SOURCES:.c=.o}
scan_BIN      = apps/scan/scan
scan_LDADD    = -lz

OBJS += $(scan_OBJS)
BINS += $(scan_BIN)
