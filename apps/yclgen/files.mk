genycl_SOURCES = apps/yclgen/genycl.c
genycl_HEADERS = apps/yclgen/yclgen.h
genycl_OBJS = ${genycl_SOURCES:.c=.o}

genlycl_SOURCES = apps/yclgen/genlycl.c
genlycl_HEADERS = apps/yclgen/yclgen.h
genlycl_OBJS = ${genlycl_SOURCES:.c=.o}

yclgen_GENOBJS = \
    apps/yclgen/tokens.o \
    apps/yclgen/parser.o

yclgen_CODEGEN = \
    apps/yclgen/tokens.c \
    apps/yclgen/tokens.h \
    apps/yclgen/parser.c \
    apps/yclgen/parser.h

yclgen_GENSRCS = \
    apps/yclgen/tokens.l \
    apps/yclgen/parser.y

nodist_BINS += \
    apps/yclgen/genycl \
    apps/yclgen/genlycl \

OBJS += $(genycl_OBJS) $(genlycl_OBJS) $(yclgen_GENOBJS)
CODEGEN += $(yclgen_CODEGEN)

