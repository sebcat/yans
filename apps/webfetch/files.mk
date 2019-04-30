libcurl_CFLAGS   != pkg-config --cflags libcurl
libcurl_LDFLAGS  != pkg-config --libs libcurl

webfetch_DEPS     = lib/util/io.c lib/util/buf.c lib/util/netstring.c \
                    lib/util/zfile.c lib/net/tcpsrc.c \
                    lib/util/os.c lib/util/sandbox.c lib/ycl/opener.c \
                    lib/ycl/yclcli.c lib/ycl/yclcli_store.c lib/ycl/ycl.c \
                    lib/ycl/ycl_msg.c
webfetch_DEPSOBJS = ${webfetch_DEPS:.c=.o}
webfetch_SOURCES  = apps/webfetch/main.c apps/webfetch/fetch.c
webfetch_HEADERS  =
webfetch_OBJS     = ${webfetch_SOURCES:.c=.o}
webfetch_BIN      = apps/webfetch/webfetch
webfetch_LDADD    = ${libcurl_LDFLAGS} -lz

OBJS += $(webfetch_OBJS)
BINS += $(webfetch_BIN)
