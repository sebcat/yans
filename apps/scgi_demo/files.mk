scgi_demo_DEPS = \
    lib/util/buf.c \
    lib/util/netstring.c \
    lib/util/io.c \
    lib/net/scgi.c

scgi_demo_DEPSOBJS = ${scgi_demo_DEPS:.c=.o}

scgi_demo_SOURCES = \
    apps/scgi_demo/main.c

scgi_demo_HEADERS =

scgi_demo_OBJS = ${scgi_demo_SOURCES:.c=.o}

scgi_demo_BIN = apps/scgi_demo/scgi_demo

OBJS += $(scgi_demo_OBJS)
nodist_BINS += $(scgi_demo_BIN)
