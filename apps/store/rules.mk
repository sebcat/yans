$(store_OBJS): $(store_DEPSOBJS) $(store_SOURCES) $(store_HEADERS)

$(store_BIN): $(store_OBJS)
	$(CC) $(CFLAGS) -o $(store_BIN) ${store_DEPSOBJS} \
		$(store_OBJS) $(LDFLAGS)

