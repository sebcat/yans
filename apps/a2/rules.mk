$(a2_OBJS): $(a2_DEPSOBJS) $(a2_SOURCES) $(a2_HEADERS)

$(a2_SHLIB): $(a2_OBJS)
	$(CC) $(CFLAGS) -o $(a2_SHLIB) -shared ${a2_DEPSOBJS} $(a2_OBJS) \
		 $(LDFLAGS)

