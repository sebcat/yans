CFLAGS=-Wall -O0 -g -L3rd_party -I3rd_party -I3rd_party/libpcap
LDFLAGS=-llua -llinenoise -lpcap -lm

all: yans

.PHONY: clean test deps cleandeps

deps:
	make -C 3rd_party

yans: deps ip.c libyans.c yans.c
	$(CC) $(CFLAGS) -o yans libyans.c ip.c buf.c url.c yans.c $(LDFLAGS)

test:
	@for A in `ls *_test.lua`; do ./yans $$A; done

cleandeps:
	make -C 3rd_party clean

clean: cleandeps
	rm -f yans
