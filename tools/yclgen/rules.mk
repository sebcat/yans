$(yclgen_CODEGEN): $(yclgen_GENSRCS)
	flex -o tools/yclgen/tokens.c --header-file=tools/yclgen/tokens.h \
		tools/yclgen/tokens.l
	bison -o tools/yclgen/parser.c --defines=tools/yclgen/parser.h \
		tools/yclgen/parser.y

$(genycl_OBJS): $(genycl_HEADERS)

$(genlycl_OBJS): $(genlycl_HEADERS)

tools/yclgen/genycl: $(genycl_OBJS)
	$(CC) $(CFLAGS) -o $@ $(genycl_OBJS) $(LDFLAGS)

tools/yclgen/genlycl: $(genlycl_OBJS)
	$(CC) $(CFLAGS) -o $@ $(genlycl_OBJS) $(LDFLAGS)

