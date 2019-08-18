lib/vulnspec/reader.o: lib/vulnspec/reader.c lib/vulnspec/vulnspec.h

lib/vulnspec/vulnspec_test: $(lib_vulnspec_vulnspec_test_DEPSOBJS) \
		$(lib_vulnspec_vulnspec_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_vulnspec_vulnspec_test_DEPSOBJS) \
		$(lib_vulnspec_vulnspec_test_OBJS) $(LDFLAGS)
