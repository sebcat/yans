yans_DEPS = \
    3rd_party/lua.c \
    3rd_party/linenoise.c \
    3rd_party/lpeg.c \
    3rd_party/jansson.c \
    lib/util/ylog.c \
    lib/util/buf.c \
    lib/util/io.c \
    lib/util/u8.c \
    lib/util/eds.c \
    lib/util/sandbox.c \
    lib/util/netstring.c \
    lib/util/zfile.c \
    lib/net/punycode.c \
    lib/net/ports.c \
    lib/net/route.c \
    lib/net/eth.c \
    lib/net/url.c \
    lib/net/ip.c \
    lib/net/iface.c \
    lib/ycl/ycl.c \
    lib/ycl/ycl_msg.c \
    lib/lua/ycl.c \
    lib/lua/cgi.c \
    lib/lua/eds.c \
    lib/lua/fts.c \
    lib/lua/http.c \
    lib/lua/json.c \
    lib/lua/net.c \
    lib/lua/opts.c \
    lib/lua/yans.c \
    lib/lua/ylog.c \
    lib/lua/util.c

yans_DEPSOBJS = ${yans_DEPS:.c=.o}

yans_SOURCES = \
    apps/yans/yans.c

yans_HEADERS =

yans_YANSTESTS = \
    apps/yans/tests/json_test.yans \
    apps/yans/tests/net_test.yans \
    apps/yans/tests/ports_test.yans \
    apps/yans/tests/url_test.yans \
    apps/yans/tests/ycl_test.yans

yans_OBJS = ${yans_SOURCES:.c=.o}

yans_LDADD_Linux = -lseccomp
yans_LDADD := -lz -lm ${yans_LDADD_${UNAME_S}}

yans_BIN = apps/yans/yans

OBJS += $(yans_OBJS)
BINS += $(yans_BIN)
YANSTESTS += $(yans_YANSTESTS)
