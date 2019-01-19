$(mkreport_OBJS): $(mkreport_DEPSOBJS) $(mkreport_SOURCES) $(mkreport_HEADERS)

$(mkreport_BIN): $(mkreport_OBJS)
	$(CC) $(CFLAGS) -o $(mkreport_BIN) ${mkreport_DEPSOBJS} \
		$(mkreport_OBJS) $(LDFLAGS)

