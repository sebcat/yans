$(webfetch_OBJS): $(webfetch_DEPSOBJS) $(webfetch_SOURCES) $(webfetch_HEADERS)

apps/webfetch/main.o: apps/webfetch/main.c
	$(CC) $(CFLAGS) $(libcurl_CFLAGS) -c $< -o $@

apps/webfetch/fetch.o: apps/webfetch/fetch.c
	$(CC) $(CFLAGS) $(libcurl_CFLAGS) -c $< -o $@


$(webfetch_BIN): $(webfetch_OBJS)
	$(CC) $(CFLAGS) -o $(webfetch_BIN) ${webfetch_DEPSOBJS} \
		$(webfetch_OBJS) $(LDFLAGS) $(webfetch_LDADD)

