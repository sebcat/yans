lib_vulnmatch_SOURCES = lib/vulnmatch/reader.c lib/vulnmatch/parser.c \
    lib/vulnmatch/progn.c
lib_vulnmatch_OBJS = ${lib_vulnmatch_SOURCES:.c=.o}

lib_vulnmatch_vulnmatch_test_DEPS = $(lib_vulnmatch_SOURCES) lib/util/buf.c \
    lib/util/hexdump.c lib/util/objtbl.c
lib_vulnmatch_vulnmatch_test_DEPSOBJS = ${lib_vulnmatch_vulnmatch_test_DEPS:.c=.o}
lib_vulnmatch_vulnmatch_test_SOURCES = lib/vulnmatch/vulnmatch_test.c
lib_vulnmatch_vulnmatch_test_OBJS = ${lib_vulnmatch_vulnmatch_test_SOURCES:.c=.o}

lib_vulnmatch_CTESTSRCS = $(lib_vulnmatch_vulnmatch_test_SOURCES)
lib_vulnmatch_CTESTS = ${lib_vulnmatch_CTESTSRCS:.c=}
OBJS += ${lib_vulnmatch_OBJS} ${lib_vulnmatch_CTESTSRCS:.c=.o}
CTESTS += ${lib_vulnmatch_CTESTS}
