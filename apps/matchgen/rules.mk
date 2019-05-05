$(matchgen_OBJS): $(matchgen_DEPSOBJS) $(matchgen_SOURCES) $(matchgen_HEADERS)

$(matchgen_BIN): $(matchgen_OBJS)
	$(CC) $(CFLAGS) -o $(matchgen_BIN) ${matchgen_DEPSOBJS} \
		$(matchgen_OBJS) $(LDFLAGS)

