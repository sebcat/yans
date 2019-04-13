$(tcpctl_OBJS): $(tcpctl_DEPSOBJS) $(tcpctl_SOURCES) $(tcpctl_HEADERS)

$(tcpctl_BIN): $(tcpctl_OBJS)
	$(CC) $(CFLAGS) -o $(tcpctl_BIN) ${tcpctl_DEPSOBJS} \
		$(tcpctl_OBJS) $(LDFLAGS) $(tcpctl_LDADD)

