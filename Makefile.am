SUBDIRS = 3rd_party
ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -Wall -Werror

TESTS = \
    lib_net_url_test \
    lib_util_u8_test \
    apps/yans/tests/url_test.yans \
    apps/yans/tests/ip_test.yans

TEST_EXTENSIONS = .yans
YANS_LOG_COMPILER = ./yans

check_PROGRAMS = \
    lib_net_url_test \
    lib_util_u8_test \
    yans

bin_PROGRAMS = \
    yans \
    pcapd

pcapd_SOURCES = \
    lib/util/buf.c \
    lib/util/io.c \
    lib/util/os.c \
    lib/util/ylog.c \
    lib/pcapd/pcapd.c \
    apps/pcapd/pcapd.c

pcapd_LDADD = \
    $(PCAPLIB) \
    $(PTHREADLIB)

yans_SOURCES = \
    lib/util/u8.c \
    lib/util/buf.c \
    lib/net/punycode.c \
    lib/net/eth.c \
    lib/net/ip.c \
    lib/net/url.c \
    lib/lua/yans.c \
    lib/lua/ypcap.c \
    apps/yans/yans.c

yans_LDADD = \
    $(PCAPLIB) \
    -lm \
    3rd_party/liblinenoise.a \
    3rd_party/liblpeg.a \
    3rd_party/liblua.a

lib_util_u8_test_SOURCES = \
    lib/util/u8.c \
    lib/util/u8_test.c

lib_net_url_test_SOURCES = \
    lib/net/url.c \
    lib/net/url_test.c \
    lib/net/punycode.c \
    lib/util/u8.c \
    lib/util/buf.c

