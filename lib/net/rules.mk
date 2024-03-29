lib/net/dnstres.o: lib/net/dnstres.c lib/net/dnstres.h
lib/net/fcgi.o: lib/net/fcgi.c lib/net/fcgi.h
lib/net/ip.o: lib/net/ip.c lib/net/ip.h
lib/net/ports.o: lib/net/ports.c lib/net/ports.h
lib/net/punycode.o: lib/net/punycode.c lib/net/punycode.h
lib/net/url.o: lib/net/url.c lib/net/url.h lib/net/punycode.h lib/net/ip.h
lib/net/dsts.o: lib/net/ports.c lib/net/ip.c
lib/net/dsts_test: $(lib_net_dsts_test_DEPSOBJS) $(lib_net_dsts_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_net_dsts_test_DEPSOBJS) $(lib_net_dsts_test_OBJS)

lib/net/punycode_test: $(lib_net_punycode_test_DEPSOBJS) \
		$(lib_net_punycode_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_net_punycode_test_DEPSOBJS) \
		$(lib_net_punycode_test_OBJS)

lib/net/url_test: $(lib_net_url_test_DEPSOBJS) $(lib_net_url_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_net_url_test_DEPSOBJS) $(lib_net_url_test_OBJS)

lib/net/urlquery_test: $(lib_net_urlquery_test_DEPSOBJS) $(lib_net_urlquery_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_net_urlquery_test_DEPSOBJS) $(lib_net_urlquery_test_OBJS)

lib/net/scgi_test: $(lib_net_scgi_test_DEPSOBJS) $(lib_net_scgi_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_net_scgi_test_DEPSOBJS) $(lib_net_scgi_test_OBJS)
