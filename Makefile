CFLAGS=-Wall -Os -g -L3rd_party -I3rd_party -I3rd_party/libpcap
LDFLAGS=-llua -llinenoise -lpcap -lm
SRC=libyans.c ip.c buf.c url.c yans.c punycode.c u8.c
SRC+=eth.c

all: yans

.PHONY: clean test deps cleandeps

deps:
	make -C 3rd_party

yans: $(SRC)
	$(CC) $(CFLAGS) -o yans $(SRC) $(LDFLAGS)

test:
	@for A in `ls *_test.lua`; do echo $$A; ./yans $$A; done

cleandeps:
	make -C 3rd_party clean

clean: cleandeps
	rm -f yans
