genycl_SOURCES = tools/yclgen/genycl.c
genycl_HEADERS = tools/yclgen/yclgen.h
genycl_OBJS = ${genycl_SOURCES:.c=.o}

genlycl_SOURCES = tools/yclgen/genlycl.c
genlycl_HEADERS = tools/yclgen/yclgen.h
genlycl_OBJS = ${genlycl_SOURCES:.c=.o}

yclgen_GENOBJS = \
    tools/yclgen/tokens.o \
    tools/yclgen/parser.o

yclgen_CODEGEN = \
    tools/yclgen/tokens.c \
    tools/yclgen/tokens.h \
    tools/yclgen/parser.c \
    tools/yclgen/parser.h

yclgen_GENSRCS = \
    tools/yclgen/tokens.l \
    tools/yclgen/parser.y

nodist_BINS += \
    tools/yclgen/genycl \
    tools/yclgen/genlycl \

OBJS += $(genycl_OBJS) $(genlycl_OBJS) $(yclgen_GENOBJS)
CODEGEN += $(yclgen_CODEGEN)

