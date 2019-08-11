vulngen_DEPS     = lib/vulnmatch/parser.c lib/vulnmatch/reader.c \
                   lib/vulnmatch/progn.c lib/util/buf.c \
		   lib/util/objtbl.c lib/util/vaguever.c
vulngen_DEPSOBJS = ${vulngen_DEPS:.c=.o}
vulngen_SOURCES  = apps/vulngen/main.c
vulngen_HEADERS  =
vulngen_OBJS     = ${vulngen_SOURCES:.c=.o}
vulngen_BIN      = apps/vulngen/vulngen
vulngen_LDADD    =

OBJS        += $(vulngen_OBJS)
nodist_BINS += $(vulngen_BIN)
