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
    lib/util/buf.h \
    lib/util/io.c \
    lib/util/io.h \
    lib/util/os.c \
    lib/util/os.h \
    lib/util/ylog.c \
    lib/util/ylog.h \
    lib/util/netstring.c \
    lib/util/netstring.h \
    lib/util/eds.c \
    lib/util/eds.h \
    lib/net/eth.c \
    lib/net/eth.h \
    lib/net/iface.c \
    lib/net/iface.h \
    proto/proto.h \
    proto/pcap_req.c \
    proto/pcap_req.h \
    proto/ethframe_req.c \
    proto/ethframe_req.h \
    proto/sweeper_req.c \
    proto/sweeper_req.h \
    proto/status_resp.c \
    proto/status_resp.h \
    apps/ethd/pcap.c \
    apps/ethd/pcap.h \
    apps/ethd/ethframe.c \
    apps/ethd/ethframe.h \
    apps/ethd/sweeper.c \
    apps/ethd/sweeper.h \
    apps/ethd/sender.c \
    apps/ethd/sender.h \
    apps/ethd/main.c

ethd_LDADD = \
    $(CAPLIB) \
    $(PCAPLIB)

yans_SOURCES = \
    3rd_party/linenoise.c \
    3rd_party/linenoise.h \
    3rd_party/lpeg.c \
    3rd_party/lpeg.h \
    3rd_party/lua.c \
    3rd_party/lua.h \
    lib/util/u8.c \
    lib/util/u8.h \
    lib/util/buf.c \
    lib/util/buf.h \
    lib/util/io.c \
    lib/util/io.h \
    lib/util/fio.c \
    lib/util/fio.h \
    lib/util/netstring.c \
    lib/util/netstring.h \
    lib/net/punycode.c \
    lib/net/punycode.h \
    lib/net/eth.c \
    lib/net/eth.h \
    lib/net/ip.c \
    lib/net/ip.h \
    lib/net/url.c \
    lib/net/url.h \
    proto/proto.h \
    proto/pcap_req.c \
    proto/pcap_req.h \
    proto/status_resp.c \
    proto/status_resp.h \
    lib/lua/yans.c \
    lib/lua/yans.h \
    lib/lua/ypcap.c \
    lib/lua/ypcap.h \
    lib/lua/pcapd.c \
    lib/lua/pcapd.h \
    apps/yans/yans.c

yans_LDADD = \
    $(PCAPLIB) \
    -lm

ping_SOURCES = \
    lib/net/eth.c \
    lib/net/eth.h \
    lib/net/ethframe.c \
    lib/net/ethframe.h \
    apps/ping/ping.c

ycli_SOURCES = \
    lib/net/iface.c \
    lib/net/iface.h \
    apps/ycli/iflist.c \
    apps/ycli/iflist.h \
    apps/ycli/pcapcli.c \
    apps/ycli/pcapcli.h \
    apps/ycli/ethframecli.c \
    apps/ycli/ethframecli.h \
    apps/ycli/sweepercli.c \
    apps/ycli/sweepercli.h \
    apps/ycli/ycli.c

ycli_LDADD = \
    libycl.la

ping_LDADD = \
    $(PCAPLIB)

libycl_la_SOURCES = \
    lib/util/buf.c \
    lib/util/buf.h \
    lib/util/io.c \
    lib/util/io.h \
    lib/util/netstring.c \
    lib/util/netstring.h \
    proto/proto.h \
    proto/pcap_req.c \
    proto/pcap_req.h \
    proto/ethframe_req.c \
    proto/ethframe_req.h \
    proto/sweeper_req.c \
    proto/sweeper_req.h \
    proto/status_resp.c \
    proto/status_resp.h \
    lib/ycl/ycl.c

libycl_la_CFLAGS = $(AM_CFLAGS)

libycl_la_LDFLAGS = \
    -version-info 1:0:0 \
    -fvisibility=hidden

lib_util_netstring_test_SOURCES = \
    lib/util/netstring.c \
    lib/util/netstring.h \
    lib/util/buf.c \
    lib/util/buf.h \
    lib/util/netstring_test.c

lib_util_u8_test_SOURCES = \
    lib/util/u8.c \
    lib/util/u8.h \
    lib/util/u8_test.c

lib_util_conf_test_SOURCES = \
    lib/util/conf.c \
    lib/util/conf.h \
    lib/util/conf_test.c

lib_net_punycode_test_SOURCES = \
    lib/util/u8.c \
    lib/util/u8.h \
    lib/util/buf.c \
    lib/util/buf.h \
    lib/net/punycode.c \
    lib/net/punycode.h \
    lib/net/punycode_test.c

lib_net_url_test_SOURCES = \
    lib/net/url.c \
    lib/net/url.h \
    lib/net/punycode.c \
    lib/net/punycode.h \
    lib/util/u8.c \
    lib/util/u8.h \
    lib/util/buf.c \
    lib/util/buf.h \
    lib/net/url_test.c

lib_net_ethframe_test_SOURCES = \
    lib/net/ethframe.c \
    lib/net/ethframe.h \
    lib/net/ethframe_test.c

