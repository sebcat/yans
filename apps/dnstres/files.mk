dnstres_DEPS = \
    lib/net/dnstres.c

dnstres_DEPSOBJS = ${dnstres_DEPS:.c=.o}

dnstres_SOURCES = \
    apps/dnstres/dnstres.c

dnstres_OBJS = ${dnstres_SOURCES:.c=.o}

dnstres_LDADD = -lpthread

dnstres_BIN = apps/dnstres/dnstres

OBJS += $(dnstres_OBJS)
nodist_BINS += $(dnstres_BIN)

