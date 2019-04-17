$(deduptbl_OBJS): $(deduptbl_DEPSOBJS) $(deduptbl_SOURCES) $(deduptbl_HEADERS)

$(deduptbl_BIN): $(deduptbl_OBJS)
	$(CC) $(CFLAGS) -o $(deduptbl_BIN) ${deduptbl_DEPSOBJS} \
		$(deduptbl_OBJS) $(LDFLAGS) ${deduptbl_LDADD}

