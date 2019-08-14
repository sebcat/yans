scan_DEPS     = lib/alloc/linvar.c lib/net/dsts.c \
                lib/net/ip.c lib/net/ports.c lib/net/reaplan.c \
                lib/net/tcpproto_types.c lib/match/tcpproto.c \
                lib/net/tcpsrc.c lib/util/buf.c lib/util/csv.c \
                lib/util/idset.c lib/util/io.c lib/util/lines.c \
                lib/util/netstring.c lib/util/reorder.c lib/util/sandbox.c \
                lib/util/str.c lib/util/zfile.c lib/ycl/ycl.c \
                lib/ycl/ycl_msg.c lib/ycl/yclcli.c lib/ycl/yclcli_store.c \
                lib/ycl/yclcli_resolve.c \
                lib/util/objtbl.c lib/util/sha1.c lib/util/x509.c \
                lib/util/os.c lib/ycl/opener.c lib/match/component.c \
                lib/vulnmatch/interp.c lib/util/vaguever.c
scan_DEPS_CC  = lib/match/reset.cc
scan_DEPSOBJS = ${scan_DEPS:.c=.o} ${scan_DEPS_CC:.cc=.o}
scan_SOURCES  = apps/scan/main.c apps/scan/resolve.c apps/scan/banners.c \
                apps/scan/bgrab.c apps/scan/collate.c
scan_HEADERS  = apps/scan/resolve.h apps/scan/banners.h \
                apps/scan/bgrab.h apps/scan/collate.h
scan_OBJS     = ${scan_SOURCES:.c=.o}
scan_BIN      = apps/scan/scan
scan_LDADD    = ${re2_LDFLAGS} ${zlib_LDFLAGS} -lssl -lcrypto -lstdc++

FreeBSD_OBJS += $(scan_OBJS)
FreeBSD_BINS += $(scan_BIN)

CODEGEN += apps/scan/collate_matches.c
