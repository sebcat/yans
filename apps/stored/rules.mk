${stored_OBJS}: ${stored_DEPSOBJS} ${stored_SOURCES} ${stored_HEADERS}

${stored_BIN}: ${stored_OBJS}
	$(CC) $(CFLAGS) -o $(stored_BIN) ${stored_DEPSOBJS} $(stored_OBJS) $(LDFLAGS)
