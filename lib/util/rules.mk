$(lib_util_OBJS): $(lib_util_HEADERS)

lib/util/u8_test: $(lib_util_u8_test_DEPSOBJS) $(lib_util_u8_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_util_u8_test_DEPSOBJS) \
		$(lib_util_u8_test_OBJS) $(LDFLAGS)

lib/util/netstring_test: $(lib_util_netstring_test_DEPSOBJS) \
		$(lib_util_netstring_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_util_netstring_test_DEPSOBJS) \
		$(lib_util_netstring_test_OBJS) $(LDFLAGS)

lib/util/conf_test: $(lib_util_conf_test_DEPSOBJS) $(lib_util_conf_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_util_conf_test_DEPSOBJS) \
		$(lib_util_conf_test_OBJS) $(LDFLAGS)