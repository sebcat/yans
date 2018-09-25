grab_banners_DEPS = \
    lib/net/reaplan.c

grab_banners_DEPSOBJS = ${grab_banners_DEPS:.c=.o}

grab_banners_SOURCES = \
    apps/grab-banners/main.c

grab_banners_HEADERS =

grab_banners_OBJS = ${grab_banners_SOURCES:.c=.o}

grab_banners_BIN = apps/grab-banners/grab-banners

OBJS += $(grab_banners_OBJS)
BINS += $(grab_banners_BIN)
