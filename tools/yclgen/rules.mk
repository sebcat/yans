tools/yclgen/parser.c: tools/yclgen/parser.y
	bison -o tools/yclgen/parser.c --defines=tools/yclgen/parser.h \
		tools/yclgen/parser.y

tools/yclgen/tokens.c: tools/yclgen/tokens.l
	flex -o tools/yclgen/tokens.c --header-file=tools/yclgen/tokens.h \
		tools/yclgen/tokens.l

tools/yclgen/tokens.o: tools/yclgen/tokens.c tools/yclgen/parser.c
tools/yclgen/parser.o: tools/yclgen/parser.c tools/yclgen/tokens.c
tools/yclgen/genlycl.o: tools/yclgen/genlycl.c tools/yclgen/yclgen.h
tools/yclgen/genycl.o: tools/yclgen/genycl.c tools/yclgen/yclgen.h

tools/yclgen/genycl: tools/yclgen/tokens.o tools/yclgen/parser.o tools/yclgen/genycl.o
	$(CC) -o $@ $(CFLAGS) tools/yclgen/tokens.o tools/yclgen/parser.o tools/yclgen/genycl.o $(LDFLAGS)

tools/yclgen/genlycl: tools/yclgen/tokens.o tools/yclgen/parser.o tools/yclgen/genlycl.o
	$(CC) -o $@ $(CFLAGS) tools/yclgen/tokens.o tools/yclgen/parser.o tools/yclgen/genlycl.o $(LDFLAGS)

