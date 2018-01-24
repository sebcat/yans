${kneg_OBJS}: $(kneg_SOURCES) $(kneg_HEADERS}

$(kneg_BIN): $(kneg_DEPSOBJS) $(kneg_OBJS)
	$(CC) $(CFLAGS) -o $(kneg_BIN) ${kneg_DEPSOBJS} \
		$(kneg_OBJS) $(LDFLAGS)

