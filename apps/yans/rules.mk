$(yans_OBJS): $(yans_SOURCES) $(yans_HEADERS)

$(yans_BIN): $(yans_DEPSOBJS) $(yans_OBJS)
	$(CC) $(CFLAGS) $(yans_CFLAGS) -o $(yans_BIN) $(yans_DEPSOBJS) \
		$(yans_OBJS) $(LDFLAGS) $(yans_LDADD)
