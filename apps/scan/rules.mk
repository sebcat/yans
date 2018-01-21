${scan_OBJS}: $(scan_SOURCES) $(scan_HEADERS}

$(scan_BIN): $(scan_DEPSOBJS) $(scan_OBJS)
	$(CC) $(CFLAGS) -o $(scan_BIN) ${scan_DEPSOBJS} \
		$(scan_OBJS) $(LDFLAGS)

