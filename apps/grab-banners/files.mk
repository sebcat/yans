grab_banners_DEPS = \
    lib/net/dsts.c \
    lib/net/ip.c \
    lib/net/ports.c \
    lib/net/reaplan.c \
    lib/net/tcpsrc.c \
    lib/util/buf.c \
    lib/util/idset.c \
    lib/util/reorder.c \
    lib/util/sandbox.c \

grab_banners_DEPSOBJS = ${grab_banners_DEPS:.c=.o}

grab_banners_SOURCES = \
    apps/grab-banners/main.c

grab_banners_HEADERS =

grab_banners_OBJS = ${grab_banners_SOURCES:.c=.o}

grab_banners_BIN = apps/grab-banners/grab-banners

OBJS += $(grab_banners_OBJS)
BINS += $(grab_banners_BIN)
