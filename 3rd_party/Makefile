CFLAGS=-Wall -Os
AR=ar rc
RANLIB=ranlib

LIBPCAPDIR=libpcap-1.7.4

all: liblua.a liblinenoise.a libpcap.a

liblua.a: lua.c lua.h
	$(CC) $(CFLAGS) -c lua.c
	$(AR) $@ lua.o
	$(RANLIB) $@

liblinenoise.a:
	$(CC) $(CFLAGS) -c linenoise.c
	$(AR) $@ linenoise.o
	$(RANLIB) $@

libpcap.a:
	cd $(LIBPCAPDIR) && ./configure --enable-ipv6=yes --enable-shared=no --enable-usb=no --enable-bluetooth=no --enable-canusb=no --enable-can=no --enable-dbus=no --enable-packet-ring=no
	cd $(LIBPCAPDIR) && make
	cp $(LIBPCAPDIR)/libpcap.a .

cleandeps:
	cd $(LIBPCAPDIR) && make distclean

clean: cleandeps
	rm -f liblua.a lua.o liblinenoise.a linenoise.a
