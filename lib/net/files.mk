lib_net_SOURCES = \
    lib/net/dnstres.c \
    lib/net/dsts.c \
    lib/net/eth.c \
    lib/net/fcgi.c \
    lib/net/scgi.c \
    lib/net/ip.c \
    lib/net/ports.c \
    lib/net/punycode.c \
    lib/net/reaplan.c \
    lib/net/sconn.c \
    lib/net/tcpproto_types.c \
    lib/net/tcpsrc.c \
    lib/net/url.c \
    lib/net/urlquery.c

lib_net_HEADERS = \
    lib/net/dnstres.h \
    lib/net/dsts.h \
    lib/net/eth.h \
    lib/net/fcgi.h \
    lib/net/scgi.h \
    lib/net/ip.h \
    lib/net/ports.h \
    lib/net/punycode.h \
    lib/net/reaplan.h \
    lib/net/sconn.h \
    lib/net/tcpproto_types.h \
    lib/net/tcpsrc.h \
    lib/net/url.h \
    lib/net/urlquery.h

lib_net_CTESTSRCS = \
    lib/net/dsts_test.c \
    lib/net/punycode_test.c \
    lib/net/url_test.c \
    lib/net/urlquery_test.c \
    lib/net/scgi_test.c

lib_net_dsts_test_DEPS     =  lib/util/buf.c lib/net/ip.c lib/net/dsts.c \
                              lib/util/reorder.c lib/net/ports.c
lib_net_dsts_test_DEPSOBJS = ${lib_net_dsts_test_DEPS:.c=.o}
lib_net_dsts_test_SOURCES  = lib/net/dsts_test.c
lib_net_dsts_test_OBJS     = ${lib_net_dsts_test_SOURCES:.c=.o}

lib_net_punycode_test_DEPS     = lib/util/buf.c lib/util/u8.c \
                                 lib/net/punycode.c
lib_net_punycode_test_DEPSOBJS = ${lib_net_punycode_test_DEPS:.c=.o}
lib_net_punycode_test_SOURCES  = lib/net/punycode_test.c
lib_net_punycode_test_OBJS     = ${lib_net_punycode_test_SOURCES:.c=.o}

lib_net_url_test_DEPS     = lib/util/buf.c lib/util/u8.c \
                            lib/util/reorder.c lib/net/punycode.c \
                            lib/net/ip.c lib/net/url.c
lib_net_url_test_DEPSOBJS = ${lib_net_url_test_DEPS:.c=.o}
lib_net_url_test_SOURCES  = lib/net/url_test.c
lib_net_url_test_OBJS     = ${lib_net_url_test_SOURCES:.c=.o}

lib_net_urlquery_test_DEPS     = lib/net/urlquery.c
lib_net_urlquery_test_DEPSOBJS = ${lib_net_urlquery_test_DEPS:.c=.o}
lib_net_urlquery_test_SOURCES  = lib/net/urlquery_test.c
lib_net_urlquery_test_OBJS     = ${lib_net_urlquery_test_SOURCES:.c=.o}

lib_net_scgi_test_DEPS     = lib/util/buf.c lib/util/netstring.c \
                             lib/util/io.c lib/net/scgi.c
lib_net_scgi_test_DEPSOBJS = ${lib_net_scgi_test_DEPS:.c=.o}
lib_net_scgi_test_SOURCES  = lib/net/scgi_test.c
lib_net_scgi_test_OBJS     = ${lib_net_scgi_test_SOURCES:.c=.o}

lib_net_CTESTS = ${lib_net_CTESTSRCS:.c=}
lib_net_OBJS   = ${lib_net_SOURCES:.c=.o}

OBJS += $(lib_net_OBJS) $(lib_net_url_test_OBJS) \
        $(lib_net_urlquery_test_OBJS) \
        $(lib_net_punycode_test_OBJS) $(lib_net_scgi_test_OBJS) \
        $(lib_net_dsts_test_OBJS) $(lib_net_tcpproto_test_OBJS)

CTESTS += $(lib_net_CTESTS)
