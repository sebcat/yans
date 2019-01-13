$(sysinfoapi_OBJS): $(sysinfoapi_DEPSOBJS) $(sysinfoapi_SOURCES) $(sysinfoapi_HEADERS)

$(sysinfoapi_BIN): $(sysinfoapi_OBJS)
	$(CC) $(CFLAGS) -o $(sysinfoapi_BIN) ${sysinfoapi_DEPSOBJS} \
		$(sysinfoapi_OBJS) $(LDFLAGS)

