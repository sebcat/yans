$(sc2_OBJS): $(sc2_DEPSOBJS) $(sc2_SOURCES) $(sc2_HEADERS)

$(sc2_BIN): $(sc2_OBJS)
	$(CC) $(CFLAGS) -o $(sc2_BIN) ${sc2_DEPSOBJS} \
		$(sc2_OBJS) $(LDFLAGS) $(sc2_LDADD)

