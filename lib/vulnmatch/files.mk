lib_vulnmatch_SOURCES = \
  lib/vulnmatch/reader.c

lib_vulnmatch_OBJS = ${lib_vulnmatch_SOURCES:.c=.o}

lib_vulnmatch_reader_test_DEPS = lib/vulnmatch/reader.c lib/util/buf.c
lib_vulnmatch_reader_test_DEPSOBJS = ${lib_vulnmatch_reader_test_DEPS:.c=.o}
lib_vulnmatch_reader_test_SOURCES = lib/vulnmatch/reader_test.c
lib_vulnmatch_reader_test_OBJS = ${lib_vulnmatch_reader_test_SOURCES:.c=.o}

lib_vulnmatch_CTESTSRCS = \
    $(lib_vulnmatch_reader_test_SOURCES)

lib_vulnmatch_CTESTS = ${lib_vulnmatch_CTESTSRCS:.c=}
OBJS += ${lib_vulnmatch_OBJS} ${lib_vulnmatch_CTESTSRCS:.c=.o}
CTESTS += ${lib_vulnmatch_CTESTS}
