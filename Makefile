# built executables that does not get installed
nodist_BINS =

# executables that has no build step (install only)
script_BINS =

# executables that gets built and installed
BINS =

# intermediate build files, does not get installed and are removed on clean
OBJS =

# generated code, gets removed on distclean
CODEGEN =

# startup files that get installed
RCFILES =

# generated startup files that get installed and are removed on clean
GENERATED_RCFILES =

# .yans test files, executed on check
YANSTESTS =

UNAME_S != uname -s
INSTALL = install
STRIP = strip -s

DESTDIR ?=
RCFILESDIR ?= /usr/local/etc/rc.d
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATAROOTDIR ?= $(PREFIX)/share
LOCALSTATEDIR ?= /var

CFLAGS ?= -Os -pipe
CFLAGS += -Wall -Werror -Wformat -Wformat-security -I.
CFLAGS += -DBINDIR=\"$(BINDIR)\" -DDATAROOTDIR=\"$(DATAROOTDIR)\"
CFLAGS += -DLOCALSTATEDIR=\"$(LOCALSTATEDIR)\"

# set MAYBE_VALGRIND to the valgrind command if USE_VALGRIND is set to 1
# used for make check
MAYBE_VALGRIND_1 = valgrind --error-exitcode=1 --leak-check=full
MAYBE_VALGRIND := ${MAYBE_VALGRIND_${USE_VALGRIND}}

.PHONY: all clean distclean check manifest manifest-rcfiles install \
     install-strip

include files.mk

all: $(nodist_BINS) $(BINS) $(GENERATED_RCFILES)

clean:
	rm -f $(nodist_BINS) $(BINS)
	rm -f $(CTESTS)
	rm -f $(OBJS)
	rm -f $(GENERATED_RCFILES)

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

manifest:
	@for B in $(BINS) $(script_BINS); do \
		B=$$(basename $$B); \
		echo $(DESTDIR)$(BINDIR)/$$B; \
	done

manifest-rcfiles:
	@for RC in $(RCFILES) $(GENERATED_RCFILES); do \
		RC=$$(basename $$RC); \
		echo $(DESTDIR)$(RCFILESDIR)/$$RC; \
	done

install: $(nodist_BINS) $(BINS)
	mkdir -p $(DESTDIR)$(BINDIR)
	for B in $(BINS) $(script_BINS); do \
		$(INSTALL) $$B $(DESTDIR)$(BINDIR); \
    done
	mkdir -p $(DESTDIR)$(DATAROOTDIR)
	cp -R lib/yans $(DESTDIR)$(DATAROOTDIR)

install-strip: install
	for B in $(BINS); do \
		B=$$(basename $$B); \
		$(STRIP) $(DESTDIR)$(BINDIR)/$$B; \
	done

install-rcfiles: $(RCFILES) $(GENERATED_RCFILES)
	mkdir -p $(DESTDIR)$(RCFILESDIR)
	for RC in $(RCFILES) $(GENERATED_RCFILES); do \
		$(INSTALL) $$RC $(DESTDIR)$(RCFILESDIR); \
	done

include rules.mk
