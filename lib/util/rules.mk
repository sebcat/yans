lib/util/buf.o: lib/util/buf.c lib/util/buf.h
lib/util/eds.o: lib/util/eds.c lib/util/eds.h lib/util/io.h
lib/util/flagset.o: lib/util/flagset.c lib/util/flagset.h
lib/util/idset.o: lib/util/idset.c lib/util/idset.h
lib/util/idtbl.o: lib/util/idtbl.c lib/util/idtbl.h
lib/util/io.o: lib/util/io.c lib/util/io.h lib/util/buf.h
lib/util/netstring.o: lib/util/netstring.c lib/util/netstring.h lib/util/buf.h
lib/util/nullfd.o: lib/util/nullfd.c lib/util/nullfd.h
lib/util/os.o: lib/util/os.c lib/util/os.h
lib/util/prng.o: lib/util/prng.c lib/util/prng.h
lib/util/reorder.o: lib/util/reorder.c lib/util/reorder.h
lib/util/sandbox.o: lib/util/sandbox.c lib/util/sandbox.h
lib/util/sindex.o: lib/util/sindex.c lib/util/sindex.h
lib/util/str.o: lib/util/str.c lib/util/str.h
lib/util/u8.o: lib/util/u8.c lib/util/u8.h
lib/util/ylog.o: lib/util/ylog.c lib/util/ylog.h
lib/util/zfile.o: lib/util/zfile.c lib/util/zfile.h

lib/util/reset.o: lib/util/reset.cc
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_lib_util_reset) -c $< -o $@

lib/util/flagset_test: $(lib_util_flagset_test_DEPSOBJS) \
		$(lib_util_flagset_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_util_flagset_test_DEPSOBJS) \
		$(lib_util_flagset_test_OBJS) $(LDFLAGS)

lib/util/idtbl_test: $(lib_util_idtbl_test_DEPSOBJS) \
		$(lib_util_idtbl_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_util_idtbl_test_DEPSOBJS) \
		$(lib_util_idtbl_test_OBJS) $(LDFLAGS)

lib/util/netstring_test: $(lib_util_netstring_test_DEPSOBJS) \
		$(lib_util_netstring_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_util_netstring_test_DEPSOBJS) \
		$(lib_util_netstring_test_OBJS) $(LDFLAGS)

lib/util/os_test: $(lib_util_os_test_DEPSOBJS) \
		$(lib_util_os_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_util_os_test_DEPSOBJS) \
		$(lib_util_os_test_OBJS) $(LDFLAGS)

lib/util/u8_test: $(lib_util_u8_test_DEPSOBJS) $(lib_util_u8_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_util_u8_test_DEPSOBJS) \
		$(lib_util_u8_test_OBJS) $(LDFLAGS)

lib/util/str_test: $(lib_util_str_test_DEPSOBJS) $(lib_util_str_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_util_str_test_DEPSOBJS) \
		$(lib_util_str_test_OBJS) $(LDFLAGS)

lib/util/idset_test: $(lib_util_idset_test_DEPSOBJS) $(lib_util_idset_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_util_idset_test_DEPSOBJS) \
		$(lib_util_idset_test_OBJS) $(LDFLAGS)

