if FREEBSD_OPT
  MAYBE_FREEBSD = freebsd
endif

SUBDIRS = $(MAYBE_FREEBSD)

ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -Wall -Werror -fvisibility=hidden

TESTS = \
    lib_net_url_test \
    lib_net_punycode_test \
    lib_net_ethframe_test \
    lib_util_u8_test \
    lib_util_netstring_test \
    lib_util_conf_test \
    apps/yans/tests/url_test.yans \
    apps/yans/tests/ip_test.yans

TEST_EXTENSIONS = .yans
YANS_LOG_COMPILER = ./yans

check_PROGRAMS = \
    lib_net_url_test \
    lib_net_punycode_test \
    lib_net_ethframe_test \
    lib_util_u8_test \
    lib_util_netstring_test \
    lib_util_conf_test \
    yans

bin_PROGRAMS = \
    yans \
    ethd \
    ping \
    ycli

lib_LTLIBRARIES = libycl.la

pkginclude_HEADERS = \
    lib/ycl/ycl.h

ethd_SOURCES = \
    lib/util/buf.c \
    lib/util/io.c \
    lib/util/os.c \
    lib/util/ylog.c \
    lib/util/netstring.c \
    lib/util/eds.c \
    lib/net/eth.c \
    proto/pcap_req.c \
    proto/ethframe_req.c \
    proto/sweeper_req.c \
    proto/status_resp.c \
    apps/ethd/pcap.c \
    apps/ethd/ethframe.c \
    apps/ethd/sweeper.c \
    apps/ethd/sender.c \
    apps/ethd/main.c

ethd_LDADD = \
    $(CAPLIB) \
    $(PCAPLIB)

yans_SOURCES = \
    3rd_party/linenoise.c \
    3rd_party/lpeg.c \
    3rd_party/lua.c \
    lib/util/u8.c \
    lib/util/buf.c \
    lib/util/io.c \
    lib/util/fio.c \
    lib/util/netstring.c \
    lib/net/punycode.c \
    lib/net/eth.c \
    lib/net/ip.c \
    lib/net/url.c \
    proto/pcap_req.c \
    proto/status_resp.c \
    lib/lua/yans.c \
    lib/lua/ypcap.c \
    lib/lua/pcapd.c \
    apps/yans/yans.c

yans_LDADD = \
    $(PCAPLIB) \
    -lm

ping_SOURCES = \
    lib/net/eth.c \
    lib/net/ethframe.c \
    apps/ping/ping.c

ycli_SOURCES = \
    lib/net/iface.c \
    apps/ycli/iflist.c \
    apps/ycli/pcapcli.c \
    apps/ycli/ethframecli.c \
    apps/ycli/sweepercli.c \
    apps/ycli/ycli.c

ycli_LDADD = \
    libycl.la

ping_LDADD = \
    $(PCAPLIB)

libycl_la_SOURCES = \
    lib/util/buf.c \
    lib/util/io.c \
    lib/util/netstring.c \
    proto/pcap_req.c \
    proto/ethframe_req.c \
    proto/sweeper_req.c \
    proto/status_resp.c \
    lib/ycl/ycl.c

libycl_la_CFLAGS = $(AM_CFLAGS)

libycl_la_LDFLAGS = \
    -version-info 1:0:0 \
    -fvisibility=hidden

lib_util_netstring_test_SOURCES = \
    lib/util/netstring.c \
    lib/util/buf.c \
    lib/util/netstring_test.c

lib_util_u8_test_SOURCES = \
    lib/util/u8.c \
    lib/util/u8_test.c

lib_util_conf_test_SOURCES = \
    lib/util/conf.c \
    lib/util/conf_test.c

lib_net_punycode_test_SOURCES = \
    lib/util/u8.c \
    lib/util/buf.c \
    lib/net/punycode.c \
    lib/net/punycode_test.c

lib_net_url_test_SOURCES = \
    lib/net/url.c \
    lib/net/url_test.c \
    lib/net/punycode.c \
    lib/util/u8.c \
    lib/util/buf.c

lib_net_ethframe_test_SOURCES = \
    lib/net/ethframe.c \
    lib/net/ethframe_test.c

