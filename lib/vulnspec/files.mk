lib_vulnspec_SOURCES = lib/vulnspec/reader.c lib/vulnspec/parser.c \
    lib/vulnspec/progn.c lib/vulnspec/interp.c
lib_vulnspec_OBJS = ${lib_vulnspec_SOURCES:.c=.o}

lib_vulnspec_vulnspec_test_DEPS = $(lib_vulnspec_SOURCES) lib/util/buf.c \
    lib/util/hexdump.c lib/util/objtbl.c lib/util/vaguever.c lib/util/nalphaver.c
lib_vulnspec_vulnspec_test_DEPSOBJS = ${lib_vulnspec_vulnspec_test_DEPS:.c=.o}
lib_vulnspec_vulnspec_test_SOURCES = lib/vulnspec/vulnspec_test.c
lib_vulnspec_vulnspec_test_OBJS = ${lib_vulnspec_vulnspec_test_SOURCES:.c=.o}

lib_vulnspec_CTESTSRCS = $(lib_vulnspec_vulnspec_test_SOURCES)
lib_vulnspec_CTESTS = ${lib_vulnspec_CTESTSRCS:.c=}
OBJS += ${lib_vulnspec_OBJS} ${lib_vulnspec_CTESTSRCS:.c=.o}
CTESTS += ${lib_vulnspec_CTESTS}
