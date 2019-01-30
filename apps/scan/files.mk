scan_DEPS     =
scan_DEPSOBJS = ${scan_DEPS:.c=.o}
scan_SOURCES  = apps/scan/main.c
scan_HEADERS  =
scan_OBJS     = ${scan_SOURCES:.c=.o}
scan_BIN      = apps/scan/scan

OBJS += $(scan_OBJS)
BINS += $(scan_BIN)
