tresdns_SOURCES = \
    apps/dnstres/resolver.c \
    apps/dnstres/tresdns.c

tresdns_HEADERS = \
    apps/dnstres/resolver.h

tresdns_OBJS = ${tresdns_SOURCES:.c=.o}

tresdns_LDADD = -lpthread

nodist_BINS += apps/dnstres/tresdns
OBJS += $(tresdns_OBJS)

