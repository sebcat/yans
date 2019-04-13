tcpctl_DEPS     = lib/net/ip.c lib/util/buf.c lib/util/reorder.c
tcpctl_DEPSOBJS = ${tcpctl_DEPS:.c=.o}
tcpctl_SOURCES  = apps/tcpctl/main.c
tcpctl_HEADERS  =
tcpctl_OBJS     = ${tcpctl_SOURCES:.c=.o}
tcpctl_LDADD    =

tcpctl_BIN       = apps/tcpctl/tcpctl

OBJS      += $(tcpctl_OBJS)
BINS      += $(tcpctl_BIN)
