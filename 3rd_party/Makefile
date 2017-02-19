# XXX: We should move to automake here

CFLAGS+=-Wall -Os
AR=ar rc
RANLIB=ranlib

.PHONY: clean cleandeps check maintainer-clean

all: liblua.a liblinenoise.a

liblua.a: lua.c lua.h
	$(CC) $(CFLAGS) -c lua.c
	$(AR) $@ lua.o
	$(RANLIB) $@

liblinenoise.a:
	$(CC) $(CFLAGS) -c linenoise.c
	$(AR) $@ linenoise.o
	$(RANLIB) $@

clean:
	rm -f liblua.a lua.o liblinenoise.a linenoise.a

# check makes all because the toplevel Makefile.am depends on the libraries
# in here to exist
check: all

maintainer-clean:
