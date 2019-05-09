matchgen_DEPS     = lib/util/csv.c lib/util/buf.c \
                    lib/match/reset_type2str.c
matchgen_DEPSOBJS = ${matchgen_DEPS:.c=.o}
matchgen_SOURCES  = apps/matchgen/main.c
matchgen_HEADERS  =
matchgen_OBJS     = ${matchgen_SOURCES:.c=.o}
matchgen_BIN      = apps/matchgen/matchgen

OBJS += $(matchgen_OBJS)
nodist_BINS += $(matchgen_BIN)
