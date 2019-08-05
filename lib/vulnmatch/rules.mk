lib/vulnmatch/reader.o: lib/vulnmatch/reader.c lib/vulnmatch/vulnmatch.h

lib/vulnmatch/vulnmatch_test: $(lib_vulnmatch_vulnmatch_test_DEPSOBJS) \
		$(lib_vulnmatch_vulnmatch_test_OBJS)
	$(CC) $(CFLAGS) -o $@ $(lib_vulnmatch_vulnmatch_test_DEPSOBJS) \
		$(lib_vulnmatch_vulnmatch_test_OBJS) $(LDFLAGS)
