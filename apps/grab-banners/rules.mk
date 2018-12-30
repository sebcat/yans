$(grab_banners_OBJS): $(grab_banners_DEPSOBJS) \
		$(grab_banners_SOURCES) $(grab_banners_HEADERS)

$(grab_banners_BIN): $(grab_banners_OBJS)
	$(CC) $(CFLAGS) -o $(grab_banners_BIN) ${grab_banners_DEPSOBJS} \
		$(grab_banners_OBJS) $(LDFLAGS) ${grab_banners_LDADD}

