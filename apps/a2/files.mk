a2_DEPS     = lib/util/buf.c lib/util/netstring.c lib/util/io.c \
              lib/util/sindex.c lib/util/zfile.c lib/util/os.c \
              lib/ycl/ycl.c lib/ycl/ycl_msg.c lib/ycl/yclcli.c \
              lib/ycl/yclcli_kneg.c lib/ycl/yclcli_store.c lib/net/scgi.c \
              lib/net/urlquery.c lib/util/u8.c
a2_DEPSOBJS = ${a2_DEPS:.c=.o}
a2_SOURCES  = apps/a2/yapi.c apps/a2/main.c
a2_HEADERS  = apps/a2/yapi.h
a2_OBJS     = ${a2_SOURCES:.c=.o}
a2_SHLIB    = apps/a2/a2.so

a2_CFLAGS = ${jansson_CFLAGS} ${zlib_CFLAGS}
a2_LDADD  = ${jansson_LDFLAGS} ${zlib_LDFLAGS}

OBJS += $(a2_OBJS)
SHLIBS += $(a2_SHLIB)
