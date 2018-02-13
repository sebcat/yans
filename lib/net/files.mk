lib_net_SOURCES = \
    lib/net/eth.c \
    lib/net/fcgi.c \
    lib/net/iface.c \
    lib/net/ip.c \
    lib/net/ports.c \
    lib/net/punycode.c \
    lib/net/route.c \
    lib/net/dnstres.c \
    lib/net/sconn.c \
    lib/net/url.c

lib_net_HEADERS = \
    lib/net/eth.h \
    lib/net/fcgi.h \
    lib/net/iface.h \
    lib/net/ip.h \
    lib/net/ports.h \
    lib/net/punycode.h \
    lib/net/route.h \
    lib/net/dnstres.h \
    lib/net/sconn.h \
    lib/net/url.h

lib_net_CTESTSRCS = \
    lib/net/punycode_test.c \
    lib/net/url_test.c

lib_net_punycode_test_DEPS = \
    lib/util/buf.c \
    lib/util/u8.c \
    lib/net/punycode.c

lib_net_punycode_test_DEPSOBJS = ${lib_net_punycode_test_DEPS:.c=.o}

lib_net_punycode_test_SOURCES = \
    lib/net/punycode_test.c

lib_net_punycode_test_OBJS = ${lib_net_punycode_test_SOURCES:.c=.o}

lib_net_url_test_DEPS = \
    lib/util/buf.c \
    lib/util/u8.c \
    lib/net/punycode.c \
    lib/net/ip.c \
    lib/net/url.c

lib_net_url_test_DEPSOBJS = ${lib_net_url_test_DEPS:.c=.o}

lib_net_url_test_SOURCES = \
    lib/net/url_test.c

lib_net_url_test_OBJS = ${lib_net_url_test_SOURCES:.c=.o}

lib_net_CTESTS = ${lib_net_CTESTSRCS:.c=}

lib_net_OBJS = ${lib_net_SOURCES:.c=.o}

OBJS += $(lib_net_OBJS) $(lib_net_url_test_OBJS) $(lib_net_punycode_test_OBJS)
CTESTS += $(lib_net_CTESTS)
