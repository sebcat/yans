$(scan_OBJS): $(scan_DEPSOBJS) $(scan_SOURCES) $(scan_HEADERS)

apps/scan/collate_matches.c: data/pm/banners.pm apps/matchgen/matchgen
	./apps/matchgen/matchgen < ./data/pm/banners.pm banner_ > apps/scan/collate_matches.c

apps/scan/collate.o: apps/scan/collate_matches.c

$(scan_BIN): $(scan_OBJS)
	$(CC) $(CFLAGS) -o $(scan_BIN) ${scan_DEPSOBJS} \
		$(scan_OBJS) $(LDFLAGS) ${scan_LDADD}

