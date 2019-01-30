$(scan_OBJS): $(scan_DEPSOBJS) $(scan_SOURCES) $(scan_HEADERS)

$(scan_BIN): $(scan_OBJS)
	$(CC) $(CFLAGS) -o $(scan_BIN) ${scan_DEPSOBJS} \
		$(scan_OBJS) $(LDFLAGS) ${scan_LDADD}

