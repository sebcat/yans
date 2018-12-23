# built executables that does not get installed
nodist_BINS =

# executables that has no build step, install to $(BINDIR)
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

# .yans library files, installed to $(DATAROOTDIR)/yans
YANSLIB =

# kneg library files, installed to $(DATAROOTDIR)/kneg
KNEGLIB =

# Yans web front-end, installed to $(DATAROOTDIR)/yans-fe
YANS_FE =

# Section 1 man pages
MANPAGES1 =

# Kernel drivers
DRIVERS =

UNAME_S != uname -s
INSTALL = install
STRIP = strip -s
PACKAGE_VERSION != ./version.sh

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

.PHONY: all clean-drivers clean distclean check drivers manifest \
    manifest-rcfiles  install install-drivers install-strip \
    install-rcfiles install-docs

include files.mk

all: $(nodist_BINS) $(BINS) $(GENERATED_RCFILES) $(YANSLIB) $(KNEGLIB) \
	$(YANS_FE) drivers

drivers:
	@for D in $(DRIVERS); do \
		make -C $$D; \
	done

clean-drivers:
	@for D in $(DRIVERS); do \
		make -C $$D clean; \
	done

clean: clean-drivers
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
	@for Y in $(YANSLIB); do \
		echo $(DESTDIR)$(DATAROOTDIR)/yans/$${Y#data/yans/}; \
	done
	@for K in $(KNEGLIB); do \
		echo $(DESTDIR)$(DATAROOTDIR)/kneg/$${K#data/kneg/}; \
	done
	@for K in $(YANS_FE); do \
		echo $(DESTDIR)$(DATAROOTDIR)/yans-fe/$${K#data/yans-fe/}; \
	done

manifest-rcfiles:
	@for RC in $(RCFILES) $(GENERATED_RCFILES); do \
		RC=$$(basename $$RC); \
		echo $(DESTDIR)$(RCFILESDIR)/$$RC; \
	done

install: $(nodist_BINS) $(BINS) $(YANSLIB) $(KNEGLIB) $(YANS_FE)
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(DATAROOTDIR)/yans
	mkdir -p $(DESTDIR)$(DATAROOTDIR)/kneg
	mkdir -p $(DESTDIR)$(DATAROOTDIR)/yans-fe
	for B in $(BINS) $(script_BINS); do \
		$(INSTALL) $$B $(DESTDIR)$(BINDIR); \
    done
	for Y in $(YANSLIB); do \
		mkdir -p $(DESTDIR)$(DATAROOTDIR)/yans/$$(dirname $${Y#data/yans/}); \
		$(INSTALL) $$Y $(DESTDIR)$(DATAROOTDIR)/yans/$${Y#data/yans/}; \
	done
	for K in $(KNEGLIB); do \
		$(INSTALL) $$K $(DESTDIR)$(DATAROOTDIR)/kneg/$${K#data/kneg/}; \
	done
	for K in $(YANS_FE); do \
		$(INSTALL) $$K $(DESTDIR)$(DATAROOTDIR)/yans-fe/$${K#data/yans-fe/}; \
	done

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

install-docs: $(MANPAGES1)
	# TODO: Implement

install-drivers:
	@for D in $(DRIVERS); do \
		make -C $$D install; \
	done

include rules.mk
