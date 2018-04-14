$(scgi_demo_OBJS): $(scgi_demo_DEPSOBJS) $(scgi_demo_SOURCES) $(scgi_demo_HEADERS)

$(scgi_demo_BIN): $(scgi_demo_OBJS)
	$(CC) $(CFLAGS) -o $(scgi_demo_BIN) ${scgi_demo_DEPSOBJS} \
		$(scgi_demo_OBJS) $(LDFLAGS)

