apps/yclgen/parser.c: apps/yclgen/parser.y
	bison -o apps/yclgen/parser.c --defines=apps/yclgen/parser.h \
		apps/yclgen/parser.y

apps/yclgen/tokens.c: apps/yclgen/tokens.l
	flex -o apps/yclgen/tokens.c --header-file=apps/yclgen/tokens.h \
		apps/yclgen/tokens.l

apps/yclgen/tokens.o: apps/yclgen/tokens.c apps/yclgen/parser.c
apps/yclgen/parser.o: apps/yclgen/parser.c apps/yclgen/tokens.c
apps/yclgen/genlycl.o: apps/yclgen/genlycl.c apps/yclgen/yclgen.h
apps/yclgen/genycl.o: apps/yclgen/genycl.c apps/yclgen/yclgen.h

apps/yclgen/genycl: apps/yclgen/tokens.o apps/yclgen/parser.o apps/yclgen/genycl.o
	$(CC) -o $@ $(CFLAGS) apps/yclgen/tokens.o apps/yclgen/parser.o apps/yclgen/genycl.o $(LDFLAGS)

apps/yclgen/genlycl: apps/yclgen/tokens.o apps/yclgen/parser.o apps/yclgen/genlycl.o
	$(CC) -o $@ $(CFLAGS) apps/yclgen/tokens.o apps/yclgen/parser.o apps/yclgen/genlycl.o $(LDFLAGS)

