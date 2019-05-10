lib_match_SOURCES    = lib/match/tcpproto.c lib/match/reset_csv.c \
                       lib/match/reset_type2str.c lib/match/component.c
lib_match_SOURCES_CC = lib/match/reset.cc

lib_match_reset_CXXFLAGS = ${re2_CXXFLAGS}
lib_match_LDFLAGS        = ${re2_LDFLAGS} -lstdc++

lib_match_reset_test_DEPS = lib/match/reset.cc lib/match/reset_csv.c \
                            lib/util/csv.c lib/util/buf.c \
                            lib/match/reset_type2str.c
lib_match_reset_test_DEPSOBJS = lib/match/reset.o lib/match/reset_csv.o \
                                lib/util/csv.o lib/util/buf.o \
                            lib/match/reset_type2str.c
lib_match_reset_test_SOURCES = lib/match/reset_test.c
lib_match_reset_test_OBJS = ${lib_match_reset_test_SOURCES:.c=.o}

lib_match_tcpproto_test_DEPS     = lib/match/tcpproto.c \
                                   lib/match/reset.cc \
                                   lib/net/tcpproto_types.c 
lib_match_tcpproto_test_DEPSOBJS = lib/match/tcpproto.o \
                                   lib/match/reset.o \
                                   lib/net/tcpproto_types.o
lib_match_tcpproto_test_SOURCES  = lib/match/tcpproto_test.c
lib_match_tcpproto_test_OBJS     = ${lib_match_tcpproto_test_SOURCES:.c=.o}

lib_match_OBJS = ${lib_match_SOURCES:.c=.o} ${lib_match_SOURCES_CC:.cc=.o}
lib_match_CTESTSRCS = $(lib_match_tcpproto_test_SOURCES) \
                      $(lib_match_reset_test_SOURCES)
lib_match_CTESTS = ${lib_match_CTESTSRCS:.c=}

OBJS += ${lib_match_OBJS} ${lib_match_CTESTSRCS:.c=.o}
CTESTS += ${lib_match_CTESTS}
