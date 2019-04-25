$(webfetch_OBJS): $(webfetch_DEPSOBJS) $(webfetch_SOURCES) $(webfetch_HEADERS)

$(webfetch_BIN): $(webfetch_OBJS)
	$(CC) $(CFLAGS) -o $(webfetch_BIN) ${webfetch_DEPSOBJS} \
		$(webfetch_OBJS) $(LDFLAGS)

