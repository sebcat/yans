$(knegd_OBJS): $(knegd_DEPSOBJS) $(knegd_SOURCES) $(knegd_HEADERS)

$(knegd_BIN): $(knegd_OBJS)
	$(CC) $(CFLAGS) -o $(knegd_BIN) ${knegd_DEPSOBJS} \
		$(knegd_OBJS) $(LDFLAGS)

