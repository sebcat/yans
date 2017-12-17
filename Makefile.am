if FREEBSD_OPT
  MAYBE_FREEBSD = freebsd
endif

SUBDIRS = $(MAYBE_FREEBSD) tools/yclgen

EXTRA_DIST = \
    lib/lua/ycl.c.in \
    lib/ycl/ycl_msg.ycl

CLEANFILES = \
    lib/lua/ycl.c \
    lib/ycl/ycl_msg.c \
    lib/ycl/ycl_msg.h

ACLOCAL_AMFLAGS = -I m4

AM_YFLAGS = -d

AM_CFLAGS = \
    -Wall \
    -Werror \
    -fvisibility=hidden \
    -Wl,--build-id=none \
    -ffunction-sections \
    -fdata-sections \
    -Wl,--gc-sections \
    -DLOCALSTATEDIR='"$(localstatedir)"'

TESTS = \
    lib_net_url_test \
    lib_net_punycode_test \
    lib_util_u8_test \
    lib_util_os_test \
    lib_util_netstring_test \
    lib_util_conf_test \
    apps/yans/tests/url_test.yans \
    apps/yans/tests/json_test.yans \
    apps/yans/tests/net_test.yans \
    apps/yans/tests/ports_test.yans \
    apps/yans/tests/ycl_test.yans

TEST_EXTENSIONS = .yans
YANS_LOG_COMPILER = ./yans

check_PROGRAMS = \
    lib_net_url_test \
    lib_net_punycode_test \
    lib_util_u8_test \
    lib_util_os_test \
    lib_util_netstring_test \
    lib_util_conf_test \
    yans

bin_PROGRAMS = \
    yans \
    ethd \
    fc2 \
    clid

lib_LTLIBRARIES = \
    libycl.la \
    libyans.la

clid_SOURCES = \
    lib/util/ylog.h \
    lib/util/ylog.c \
    lib/util/eds.h \
    lib/util/eds.c \
    lib/util/os.h \
    lib/util/os.c \
    lib/util/buf.c \
    lib/util/buf.h \
    lib/util/netstring.c \
    lib/util/netstring.h \
    lib/util/io.c \
    lib/util/io.h \
    lib/util/prng.c \
    lib/util/prng.h \
    lib/ycl/ycl.c \
    lib/ycl/ycl.h \
    lib/ycl/ycl_msg.c \
    lib/ycl/ycl_msg.h \
    apps/clid/store.c \
    apps/clid/store.h \
    apps/clid/main.c

fc2_SOURCES = \
    lib/util/ylog.h \
    lib/util/ylog.c \
    lib/util/eds.h \
    lib/util/eds.c \
    lib/util/os.h \
    lib/util/os.c \
    lib/util/buf.c \
    lib/util/buf.h \
    lib/util/io.c \
    lib/util/io.h \
    lib/net/fcgi.h \
    lib/net/fcgi.c \
    apps/fc2/fc2.c

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
    lib/util/flagset.c \
    lib/util/flagset.h \
    lib/util/eds.c \
    lib/util/eds.h \
    lib/net/eth.c \
    lib/net/eth.h \
    lib/net/iface.c \
    lib/net/iface.h \
    lib/net/ip.h \
    lib/net/ip.c \
    lib/net/ports.h \
    lib/net/ports.c \
    apps/ethd/pcap.c \
    apps/ethd/pcap.h \
    apps/ethd/ethframe.c \
    apps/ethd/ethframe.h \
    apps/ethd/main.c

ethd_LDADD = \
    $(CAPLIB) \
    $(PCAPLIB) \
    libycl.la

yans_SOURCES = \
    apps/yans/yans.c

yans_LDADD = \
    $(PCAPLIB) \
    $(SECCOMPLIB) \
    libycl.la \
    libyans.la \
    -lm

libyans_la_SOURCES = \
    3rd_party/linenoise.c \
    3rd_party/linenoise.h \
    3rd_party/lpeg.c \
    3rd_party/lpeg.h \
    3rd_party/lua.c \
    3rd_party/lua.h \
    3rd_party/jansson.h \
    3rd_party/jansson.c \
    lib/util/u8.c \
    lib/util/u8.h \
    lib/util/buf.c \
    lib/util/buf.h \
    lib/util/io.c \
    lib/util/io.h \
    lib/util/sandbox.c \
    lib/util/sandbox.h \
    lib/util/netstring.c \
    lib/util/netstring.h \
    lib/util/ylog.h \
    lib/util/ylog.c \
    lib/util/eds.h \
    lib/util/eds.c \
    lib/net/punycode.c \
    lib/net/punycode.h \
    lib/net/eth.c \
    lib/net/eth.h \
    lib/net/iface.c \
    lib/net/iface.h \
    lib/net/ip.c \
    lib/net/ip.h \
    lib/net/ports.c \
    lib/net/ports.h \
    lib/net/url.c \
    lib/net/url.h \
    lib/net/route.c \
    lib/net/route.h \
    lib/ycl/ycl.h \
    lib/lua/net.c \
    lib/lua/net.h \
    lib/lua/http.c \
    lib/lua/http.h \
    lib/lua/ypcap.c \
    lib/lua/ypcap.h \
    lib/lua/json.h \
    lib/lua/json.c \
    lib/lua/cgi.h \
    lib/lua/cgi.c \
    lib/lua/fts.h \
    lib/lua/fts.c \
    lib/lua/opts.h \
    lib/lua/opts.c \
    lib/lua/ylog.h \
    lib/lua/ylog.c \
    lib/lua/eds.h \
    lib/lua/eds.c \
    lib/lua/ycl.h \
    lib/lua/yans.h \
    lib/lua/yans.c

nodist_libyans_la_SOURCES = \
    lib/lua/ycl.c

lib/lua/ycl.c: lib/lua/ycl.c.in lib/ycl/ycl_msg.ycl
	@./tools/yclgen/genlycl -f lib/ycl/ycl_msg.ycl < lib/lua/ycl.c.in \
			> lib/lua/ycl.c

libyans_la_LDFLAGS = \
    -version-info 1:0:0 \
    -fvisibility=hidden

libyans_la_CFLAGS = $(AM_CFLAGS)

libycl_la_SOURCES = \
    lib/util/buf.c \
    lib/util/buf.h \
    lib/util/io.c \
    lib/util/io.h \
    lib/util/netstring.c \
    lib/util/netstring.h \
    lib/ycl/ycl.c

nodist_libycl_la_SOURCES = \
    lib/ycl/ycl_msg.c \
    lib/ycl/ycl_msg.h

lib/ycl/ycl_msg.c lib/ycl/ycl_msg.h: tools/yclgen/genycl lib/ycl/ycl_msg.ycl
	./tools/yclgen/genycl lib/ycl/ycl_msg.h lib/ycl/ycl_msg.c \
			< lib/ycl/ycl_msg.ycl

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

lib_util_os_test_SOURCES = \
    lib/util/os.c \
    lib/util/os.h \
    lib/util/os_test.c

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
    lib/net/ip.h \
    lib/net/ip.c \
    lib/util/u8.c \
    lib/util/u8.h \
    lib/util/buf.c \
    lib/util/buf.h \
    lib/net/url_test.c
