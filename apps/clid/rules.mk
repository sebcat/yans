$(clid_OBJS): $(clid_DEPSOBJS) $(clid_SOURCES) $(clid_HEADERS)

$(clid_BIN): $(clid_OBJS)
	$(CC) $(CFLAGS) -o $(clid_BIN) ${clid_DEPSOBJS} $(clid_OBJS) $(LDFLAGS)

