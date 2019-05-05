
lib/match/tcpproto.o: lib/match/tcpproto.c lib/match/tcpproto.h

lib/match/reset.o: lib/match/reset.cc lib/match/reset.h
	$(CXX) $(CXXFLAGS) $(lib_match_reset_CXXFLAGS) -c $< -o $@

lib/match/tcpproto_test: $(lib_match_tcpproto_test_DEPSOBJS) $(lib_match_tcpproto_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_match_tcpproto_test_DEPSOBJS) \
		 $(lib_match_tcpproto_test_OBJS) $(LDFLAGS) $(lib_match_LDFLAGS)

lib/match/reset_test: $(lib_match_reset_test_DEPSOBJS) $(lib_match_reset_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_match_reset_test_DEPSOBJS) \
		$(lib_match_reset_test_OBJS) $(LDFLAGS) \
		$(lib_match_LDFLAGS)
