${ethd_OBJS}: ${ethd_DEPSOBJS} ${ethd_SOURCES} ${ethd_HEADERS}

${ethd_BIN}: ${ethd_OBJS}
	$(CC) $(CFLAGS) -o $(ethd_BIN) ${ethd_DEPSOBJS} $(ethd_OBJS) $(LDFLAGS) \
		$(ethd_LDADD)
