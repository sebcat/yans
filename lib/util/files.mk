lib_util_SOURCES = \
    lib/util/buf.c \
    lib/util/conf.c \
    lib/util/eds.c \
    lib/util/flagset.c \
    lib/util/io.c \
    lib/util/netstring.c \
    lib/util/nullfd.c \
    lib/util/os.c \
    lib/util/os_test.c \
    lib/util/prng.c \
    lib/util/sandbox.c \
    lib/util/u8.c \
    lib/util/ylog.c \
    lib/util/zfile.c \
    lib/util/u8_test.c \
    lib/util/netstring_test.c \
    lib/util/conf_test.c \
    lib/util/flagset_test.c

lib_util_OBJS = ${lib_util_SOURCES:.c=.o}

lib_util_HEADERS = \
    lib/util/buf.h \
    lib/util/conf.h \
    lib/util/eds.h \
    lib/util/flagset.h \
    lib/util/io.h \
    lib/util/netstring.h \
    lib/util/os.h \
    lib/util/prng.h \
    lib/util/sandbox.h \
    lib/util/u8.h \
    lib/util/ylog.h \
    lib/util/zfile.h

lib_util_u8_test_DEPS = \
	lib/util/u8.c

lib_util_u8_test_DEPSOBJS = ${lib_util_u8_test_DEPS:.c=.o}

lib_util_u8_test_SOURCES = \
	lib/util/u8_test.c

lib_util_u8_test_OBJS = ${lib_util_u8_test_SOURCES:.c=.o}

lib_util_netstring_test_DEPS = \
	lib/util/buf.c \
    lib/util/netstring.c

lib_util_netstring_test_DEPSOBJS = ${lib_util_netstring_test_DEPS:.c=.o}

lib_util_netstring_test_SOURCES = \
    lib/util/netstring_test.c

lib_util_netstring_test_OBJS = ${lib_util_netstring_test_SOURCES:.c=.o}

lib_util_conf_test_DEPS = \
    lib/util/conf.c

lib_util_conf_test_DEPSOBJS = ${lib_util_conf_test_DEPS:.c=.o}

lib_util_conf_test_SOURCES = \
    lib/util/conf_test.c \

lib_util_conf_test_OBJS = ${lib_util_conf_test_SOURCES:.c=.o}

lib_util_flagset_test_DEPS = \
	lib/util/flagset.c

lib_util_flagset_test_DEPSOBJS = ${lib_util_flagset_test_DEPS:.c=.o}

lib_util_flagset_test_SOURCES = \
	lib/util/flagset_test.c

lib_util_flagset_test_OBJS = ${lib_util_flagset_test_SOURCES:.c=.o}

lib_util_CTESTSRCS = \
    lib/util/u8_test.c \
    lib/util/netstring_test.c \
    lib/util/conf_test.c \
    lib/util/flagset_test.c

lib_util_CTESTS = ${lib_util_CTESTSRCS:.c=}

OBJS += ${lib_util_OBJS}
CTESTS += ${lib_util_CTESTS}
