nodist_BINS =
BINS =
OBJS =
CODEGEN =
CFLAGS ?= -Os -pipe
CFLAGS += -I.

UNAME_S != uname -s
INSTALL = install

DESTDIR ?=
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

.PHONY: all clean distclean check check-ctests check-yanstests install

include 3rd_party/files.mk
include tools/yclgen/files.mk
include apps/yans/files.mk
include lib/util/files.mk
include lib/lua/files.mk
include lib/net/files.mk
include lib/ycl/files.mk
include apps/clid/files.mk
include apps/ethd/files.mk
include apps/fc2/files.mk

all: $(nodist_BINS) $(BINS)

clean:
	rm -f $(nodist_BINS) $(BINS)
	rm -f $(CTESTS)
	rm -f $(OBJS)

distclean: clean
	rm -f $(CODEGEN)

check: $(yans_BIN) $(YANSTESTS) $(CTESTS)
	@for T in $(CTESTS); do \
		echo $$T; \
		$(MAYBE_VALGRIND) ./$$T; \
	done
	@for T in $(YANSTESTS); do \
		echo $$T; \
		$(MAYBE_VALGRIND) $(yans_BIN) ./$$T; \
	done

install: $(nodist_BINS) $(BINS)
	mkdir -p $(DESTDIR)$(BINDIR)
	for B in $(BINS); do \
		$(INSTALL) $$B $(DESTDIR)$(BINDIR); \
    done

include 3rd_party/rules.mk
include tools/yclgen/rules.mk
include apps/yans/rules.mk
include lib/util/rules.mk
include lib/lua/rules.mk
include lib/net/rules.mk
include lib/ycl/rules.mk
include apps/clid/rules.mk
include apps/ethd/rules.mk
include apps/fc2/rules.mk
