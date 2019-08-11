$(vulngen_OBJS): $(vulngen_DEPSOBJS) $(vulngen_SOURCES) $(vulngen_HEADERS)

$(vulngen_BIN): $(vulngen_OBJS)
	$(CC) $(CFLAGS) -o $(vulngen_BIN) ${vulngen_DEPSOBJS} \
		$(vulngen_OBJS) $(LDFLAGS) $(vulngen_LDADD)

