$(lib_net_OBJS): $(lib_net_SOURCES) $(lib_net_HEADERS)

lib/net/punycode_test: $(lib_net_punycode_test_DEPSOBJS) \
		$(lib_net_punycode_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_net_punycode_test_DEPSOBJS) \
		$(lib_net_punycode_test_OBJS)

lib/net/url_test: $(lib_net_url_test_DEPSOBJS) $(lib_net_url_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_net_url_test_DEPSOBJS) $(lib_net_url_test_OBJS)
