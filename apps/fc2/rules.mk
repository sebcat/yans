${fc2_OBJS}: ${fc2_SOURCES} ${fc2_HEADERS}

${fc2_BIN}: ${fc2_DEPSOBJS} ${fc2_OBJS}
	$(CC) $(CFLAGS) -o $(fc2_BIN) ${fc2_DEPSOBJS} $(fc2_OBJS) $(LDFLAGS)
