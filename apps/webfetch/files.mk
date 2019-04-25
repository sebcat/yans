webfetch_DEPS     =
webfetch_DEPSOBJS = ${webfetch_DEPS:.c=.o}
webfetch_SOURCES  = apps/webfetch/main.c
webfetch_HEADERS  =
webfetch_OBJS     = ${webfetch_SOURCES:.c=.o}
webfetch_BIN      = apps/webfetch/webfetch

OBJS += $(webfetch_OBJS)
BINS += $(webfetch_BIN)
