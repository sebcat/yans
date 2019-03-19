$(scgi_demo_OBJS): $(scgi_demo_DEPSOBJS) $(scgi_demo_SOURCES) $(scgi_demo_HEADERS)

$(scgi_demo_SHLIB): $(scgi_demo_OBJS)
	$(CC) $(CFLAGS) -o $(scgi_demo_SHLIB) -shared ${scgi_demo_DEPSOBJS} \
		$(scgi_demo_OBJS) $(LDFLAGS)

