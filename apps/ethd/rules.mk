${ethd_OBJS}: ${ethd_SOURCES} ${ethd_HEADERS}

${ethd_BIN}: ${ethd_DEPSOBJS} ${ethd_OBJS}
	$(CC) $(CFLAGS) -o $(ethd_BIN) ${ethd_DEPSOBJS} $(ethd_OBJS) $(LDFLAGS) \
		$(ethd_LDADD)
