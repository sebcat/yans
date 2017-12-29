nodist_BINS =
BINS =
OBJS =
CODEGEN =
CFLAGS ?= -Os -pipe
CFLAGS += -Wall -Werror -I.

UNAME_S != uname -s
INSTALL = install
STRIP = strip -s

DESTDIR ?=
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
DATAROOTDIR = $(PREFIX)/share
LOCALSTATEDIR = /var

# set MAYBE_VALGRIND to the valgrind command if USE_VALGRIND is set to 1
MAYBE_VALGRIND_1 = valgrind --error-exitcode=1 --leak-check=full
MAYBE_VALGRIND := ${MAYBE_VALGRIND_${USE_VALGRIND}}

.PHONY: all clean distclean check install install-strip

include files.mk

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

install-strip: install
	for B in $(BINS); do \
		B=$$(basename $$B); \
		$(STRIP) $(DESTDIR)$(BINDIR)/$$B; \
	done

include rules.mk
