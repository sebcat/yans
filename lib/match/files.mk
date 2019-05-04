lib_match_SOURCES = lib/match/tcpproto.c
lib_match_SOURCES_CC = lib/match/reset.cc

lib_match_reset_CXXFLAGS != pkg-config --cflags re2
lib_match_reset_LDFLAGS  != pkg-config --libs re2
lib_match_reset_LDFLAGS  += -lstdc++

lib_match_tcpproto_test_LDFLAGS  != pkg-config --libs re2
lib_match_tcpproto_test_LDFLAGS  += -lstdc++

lib_match_reset_test_DEPS = lib/match/reset.cc
lib_match_reset_test_DEPSOBJS = lib/match/reset.o
lib_match_reset_test_SOURCES = lib/match/reset_test.c
lib_match_reset_test_OBJS = ${lib_match_reset_test_SOURCES:.c=.o}

lib_match_tcpproto_test_DEPS     = lib/match/tcpproto.c \
                                   lib/match/reset.cc \
                                   lib/net/tcpproto_types.c 
lib_match_tcpproto_test_DEPSOBJS = lib/match/tcpproto.o \
                                   lib/match/reset.o \
                                   lib/net/tcpproto_types.c
lib_match_tcpproto_test_SOURCES  = lib/match/tcpproto_test.c
lib_match_tcpproto_test_OBJS     = ${lib_match_tcpproto_test_SOURCES:.c=.o}

lib_match_OBJS = ${lib_match_SOURCES:.c=.o} ${lib_match_SOURCES_CC:.cc=.o}
lib_match_CTESTSRCS = $(lib_match_tcpproto_test_SOURCES) \
                      $(lib_match_reset_test_SOURCES)
lib_match_CTESTS = ${lib_match_CTESTSRCS:.c=}

OBJS += ${lib_match_OBJS} ${lib_match_CTESTSRCS:.c=.o}
CTESTS += ${lib_match_CTESTS}
