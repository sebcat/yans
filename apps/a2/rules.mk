apps/a2/main.o: apps/a2/main.c
	$(CC) $(CFLAGS) $(a2_CFLAGS) -c apps/a2/main.c -o $@

$(a2_OBJS): $(a2_DEPSOBJS) $(a2_SOURCES) $(a2_HEADERS)

$(a2_SHLIB): $(a2_OBJS)
	$(CC) $(CFLAGS) $(a2_CFLAGS) -o $(a2_SHLIB) -shared \
		${a2_DEPSOBJS} $(a2_OBJS)  $(LDFLAGS) $(a2_LDADD)

