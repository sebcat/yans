$(genmatcher_OBJS): $(genmatcher_DEPSOBJS) $(genmatcher_SOURCES) $(genmatcher_HEADERS)

$(genmatcher_BIN): $(genmatcher_OBJS)
	$(CC) $(CFLAGS) -o $(genmatcher_BIN) ${genmatcher_DEPSOBJS} \
		$(genmatcher_OBJS) $(LDFLAGS) ${genmatcher_LDADD}

