deduptbl_DEPS     = lib/util/deduptbl.c lib/util/io.c lib/util/buf.c \
                    lib/util/netstring.c lib/util/zfile.c lib/util/os.c \
                    lib/util/lines.c \
                    lib/ycl/opener.c lib/ycl/ycl.c lib/ycl/ycl_msg.c \
                    lib/ycl/yclcli.c lib/ycl/yclcli_store.c
deduptbl_DEPSOBJS = ${deduptbl_DEPS:.c=.o}
deduptbl_SOURCES  = apps/deduptbl/main.c
deduptbl_HEADERS  =
deduptbl_OBJS     = ${deduptbl_SOURCES:.c=.o}
deduptbl_BIN      = apps/deduptbl/deduptbl
deduptbl_LDADD    = -lcrypto -lz

OBJS += $(deduptbl_OBJS)
BINS += $(deduptbl_BIN)
