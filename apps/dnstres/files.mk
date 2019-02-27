dnstres_DEPS     = lib/util/buf.c lib/util/u8.c lib/net/dnstres.c \
                   lib/net/punycode.c
dnstres_DEPSOBJS = ${dnstres_DEPS:.c=.o}
dnstres_SOURCES  =  apps/dnstres/dnstres.c
dnstres_OBJS     = ${dnstres_SOURCES:.c=.o}
dnstres_LDADD    = -lpthread
dnstres_BIN      = apps/dnstres/dnstres

OBJS += $(dnstres_OBJS)
nodist_BINS += $(dnstres_BIN)

