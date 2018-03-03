lib/net/dnstres.o: lib/net/dnstres.c lib/net/dnstres.h
lib/net/eth.o: lib/net/eth.c lib/net/eth.h
lib/net/fcgi.o: lib/net/fcgi.c lib/net/fcgi.h
lib/net/iface.o: lib/net/iface.c lib/net/iface.h lib/net/ip.h
lib/net/ip.o: lib/net/ip.c lib/net/ip.h
lib/net/neigh.o: lib/net/neigh.c lib/net/neigh.h lib/net/ip.h
lib/net/ports.o: lib/net/ports.c lib/net/ports.h
lib/net/punycode.o: lib/net/punycode.c lib/net/punycode.h
lib/net/route.o: lib/net/route.c lib/net/route.h lib/net/ip.h
lib/net/sconn.o: lib/net/sconn.c lib/net/sconn.h
lib/net/url.o: lib/net/url.c lib/net/url.h lib/net/punycode.h lib/net/ip.h

lib/net/punycode_test: $(lib_net_punycode_test_DEPSOBJS) \
		$(lib_net_punycode_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_net_punycode_test_DEPSOBJS) \
		$(lib_net_punycode_test_OBJS)

lib/net/url_test: $(lib_net_url_test_DEPSOBJS) $(lib_net_url_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_net_url_test_DEPSOBJS) $(lib_net_url_test_OBJS)
