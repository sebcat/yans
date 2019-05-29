$(webfetch_OBJS): $(webfetch_DEPSOBJS) $(webfetch_SOURCES) $(webfetch_HEADERS)

apps/webfetch/modules/matcher_httpheader.c: data/pm/httpheader.pm apps/matchgen/matchgen
	./apps/matchgen/matchgen < ./data/pm/httpheader.pm httpheader_ > apps/webfetch/modules/matcher_httpheader.c

apps/webfetch/modules/matcher.o: apps/webfetch/modules/matcher.c apps/webfetch/modules/matcher_httpheader.c

apps/webfetch/main.o: apps/webfetch/main.c
	$(CC) $(CFLAGS) $(libcurl_CFLAGS) -c $< -o $@

apps/webfetch/fetch.o: apps/webfetch/fetch.c
	$(CC) $(CFLAGS) $(libcurl_CFLAGS) -c $< -o $@

$(webfetch_BIN): $(webfetch_OBJS)
	$(CC) $(CFLAGS) -o $(webfetch_BIN) ${webfetch_DEPSOBJS} \
		$(webfetch_OBJS) $(LDFLAGS) $(webfetch_LDADD)

