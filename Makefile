# built executables that does not get installed
nodist_BINS =

# executables that has no build step, install to $(BINDIR)
script_BINS =

# executables that gets built and installed
BINS =

# built shared objects that does not get installed
nodist_SHLIBS =

#shared objects that gets built and installed
SHLIBS =

# intermediate build files, does not get installed and are removed on clean
OBJS =

# generated code, gets removed on distclean
CODEGEN =

# startup files that get installed
RCFILES =

# generated startup files that get installed and are removed on clean
GENERATED_RCFILES =

# kneg library files, installed to $(DATAROOTDIR)/kneg
KNEGLIB =

# kneg MANIFEST, installed to $(DATAROOTDIR)/kneg as non-exec
KNEGMANIFEST =

# Yans web front-end, installed to $(DATAROOTDIR)/yans-fe
YANS_FE =

# Section 1 man pages
MANPAGES1 =

# Kernel drivers
DRIVERS =

# CFLAGS/CXXFLAGS for dependencies
jansson_CFLAGS   != pkg-config --cflags jansson
libcurl_CFLAGS   != pkg-config --cflags libcurl
re2_CXXFLAGS     != pkg-config --cflags re2
zlib_CFLAGS      != pkg-config --cflags zlib

# LDFLAGS for dependencies
jansson_LDFLAGS  != pkg-config --libs jansson
libcurl_LDFLAGS  != pkg-config --libs libcurl
re2_LDFLAGS      != pkg-config --libs re2
zlib_LDFLAGS     != pkg-config --libs zlib


UNAME_S != uname -s
INSTALL = install
STRIP = strip -s
PACKAGE_VERSION != ./version.sh

DESTDIR ?=
RCFILESDIR ?= /usr/local/etc/rc.d
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
DATAROOTDIR ?= $(PREFIX)/share
LOCALSTATEDIR ?= /var

CFLAGS ?= -Os -pipe
CFLAGS += -Wall -Werror -Wformat -Wformat-security -I. -fPIC -fwrapv
CFLAGS += -DBINDIR=\"$(BINDIR)\" -DDATAROOTDIR=\"$(DATAROOTDIR)\"
CFLAGS += -DLOCALSTATEDIR=\"$(LOCALSTATEDIR)\"
CFLAGS += -DLIBDIR=\"$(LIBDIR)\"

CXXFLAGS += -Wall -Werror -Wformat -Wformat-security -I. -fPIC -fwrapv
CXXFLAGS += -DBINDIR=\"$(BINDIR)\" -DDATAROOTDIR=\"$(DATAROOTDIR)\"
CXXFLAGS += -DLOCALSTATEDIR=\"$(LOCALSTATEDIR)\"
CXXFLAGS += -DLIBDIR=\"$(LIBDIR)\"

# set MAYBE_VALGRIND to the valgrind command if USE_VALGRIND is set to 1
# used for make check
MAYBE_VALGRIND_1 = valgrind --error-exitcode=1 --leak-check=full
MAYBE_VALGRIND := ${MAYBE_VALGRIND_${USE_VALGRIND}}

.PHONY: all clean-drivers clean distclean check drivers manifest \
    manifest-rcfiles  install install-drivers install-strip \
    install-rcfiles install-docs

include files.mk

OBJS   += ${${UNAME_S}_OBJS}
BINS   += ${${UNAME_S}_BINS}
SHLIBS += ${${UNAME_S}_SHLIBS}
GENERATED_RCFILES += ${GENERATED_RCFILES_${UNAME_S}}

all: $(nodist_BINS) $(BINS) $(nodist_SHLIBS) $(SHLIBS) \
	$(GENERATED_RCFILES) $(KNEGLIB) \
	$(KNEGMANIFEST) $(YANS_FE)

# driver building, installing, cleaning is done explicitly, with no
# dependencies in targets like "all", "clean", "install" since most
# compilation flags suitable for user-space apps are not suitable for
# driver compilation.

drivers:
	@for D in $(DRIVERS); do \
		make -C $$D; \
	done

clean-drivers:
	@for D in $(DRIVERS); do \
		make -C $$D clean; \
	done

install-drivers:
	@for D in $(DRIVERS); do \
		make -C $$D install; \
	done

clean:
	rm -f $(nodist_BINS) $(BINS) $(nodist_SHLIBS) $(SHLIBS)
	rm -f $(CTESTS)
	rm -f $(OBJS)
	rm -f $(GENERATED_RCFILES)

distclean: clean
	rm -f $(CODEGEN)

check: $(CTESTS)
	@for T in $(CTESTS); do \
		echo $$T; \
		ASAN_OPTIONS=detect_container_overflow=0 $(MAYBE_VALGRIND) ./$$T; \
	done

manifest:
	@for B in $(BINS) $(script_BINS); do \
		B=$$(basename $$B); \
		echo $(DESTDIR)$(BINDIR)/$$B; \
	done
	@for B in $(SHLIBS); do \
		B=$$(basename $$B); \
		echo $(DESTDIR)$(LIBDIR)/$$B; \
	done
	@for K in $(KNEGLIB); do \
		echo $(DESTDIR)$(DATAROOTDIR)/kneg/$${K#data/kneg/}; \
	done
	@for K in $(KNEGMANIFEST); do \
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

install: $(script_BINS) $(BINS) $(SHLIBS) $(KNEGLIB) \
		$(KNEGMANIFEST) $(YANS_FE)
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(LIBDIR)
	mkdir -p $(DESTDIR)$(DATAROOTDIR)/kneg
	mkdir -p $(DESTDIR)$(DATAROOTDIR)/yans-fe
	for B in $(BINS) $(script_BINS); do \
		$(INSTALL) -m 755 $$B $(DESTDIR)$(BINDIR); \
	done
	for B in $(SHLIBS); do \
		$(INSTALL) -m 755 $$B $(DESTDIR)$(LIBDIR); \
	done
	for K in $(KNEGLIB); do \
		$(INSTALL) -m 755 $$K $(DESTDIR)$(DATAROOTDIR)/kneg/$${K#data/kneg/}; \
	done
	for K in $(KNEGMANIFEST); do \
		$(INSTALL) -m 644 $$K $(DESTDIR)$(DATAROOTDIR)/kneg/$${K#data/kneg/}; \
	done
	for K in $(YANS_FE); do \
		$(INSTALL) -m 644 $$K $(DESTDIR)$(DATAROOTDIR)/yans-fe/$${K#data/yans-fe/}; \
	done

install-strip: install
	for B in $(BINS); do \
		B=$$(basename $$B); \
		$(STRIP) $(DESTDIR)$(BINDIR)/$$B; \
	done
	for B in $(SHLIBS); do \
		B=$$(basename $$B); \
		$(STRIP) $(DESTDIR)$(LIBDIR)/$$B; \
	done

install-rcfiles: $(RCFILES) $(GENERATED_RCFILES)
	mkdir -p $(DESTDIR)$(RCFILESDIR)
	for RC in $(RCFILES) $(GENERATED_RCFILES); do \
		$(INSTALL) $$RC $(DESTDIR)$(RCFILESDIR); \
	done

install-docs: $(MANPAGES1)
	# TODO: Implement

include rules.mk
