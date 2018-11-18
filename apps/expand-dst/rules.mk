$(expand_dst_OBJS): $(expand_dst_DEPSOBJS) \
		$(expand_dst_SOURCES) $(expand_dst_HEADERS)

$(expand_dst_BIN): $(expand_dst_OBJS)
	$(CC) $(CFLAGS) -o $(expand_dst_BIN) ${expand_dst_DEPSOBJS} \
		$(expand_dst_OBJS) $(LDFLAGS)

