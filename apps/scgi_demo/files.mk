scgi_demo_DEPS     = lib/util/buf.c lib/util/netstring.c lib/util/io.c \
                     lib/net/scgi.c
scgi_demo_DEPSOBJS = ${scgi_demo_DEPS:.c=.o}
scgi_demo_SOURCES  = apps/scgi_demo/main.c
scgi_demo_HEADERS  =
scgi_demo_OBJS     = ${scgi_demo_SOURCES:.c=.o}
scgi_demo_SHLIB    = apps/scgi_demo/scgi_demo.so

OBJS += $(scgi_demo_OBJS)
nodist_SHLIBS += $(scgi_demo_SHLIB)
