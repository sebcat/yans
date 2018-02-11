
$(dnstres_OBJS): $(dnstres_DEPSOBJS) $(dnstres_SOURCES) $(dnstres_HEADERS)

$(dnstres_BIN): $(dnstres_OBJS)
	$(CC) $(CFLAGS) -o $(dnstres_BIN) $(dnstres_DEPSOBJS) $(dnstres_OBJS) \
		$(LDFLAGS) $(dnstres_LDADD)
