genmatcher_DEPS     =
genmatcher_DEPS_CC  = lib/util/reset.cc
genmatcher_DEPSOBJS = ${genmatcher_DEPS:.c=.o} ${genmatcher_DEPS_CC:.cc=.o}
genmatcher_SOURCES  = apps/genmatcher/main.c
genmatcher_HEADERS  =
genmatcher_OBJS     = ${genmatcher_SOURCES:.c=.o}
genmatcher_BIN      = apps/genmatcher/genmatcher
genmatcher_LDADD   != pkg-config --libs re2
genmatcher_LDADD   += -lstdc++

OBJS        += $(genmatcher_OBJS)
nodist_BINS += $(genmatcher_BIN)
