${scand_OBJS}: $(scand_SOURCES) $(scand_HEADERS}

$(scand_BIN): $(scand_DEPSOBJS) $(scand_OBJS)
	$(CC) $(CFLAGS) -o $(scand_BIN) ${scand_DEPSOBJS} \
		$(scand_OBJS) $(LDFLAGS)

