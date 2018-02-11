
$(tresdns_OBJS): $(tresdns_SOURCES) $(tresdns_HEADERS)

apps/dnstres/tresdns: $(tresdns_OBJS)
	$(CC) $(CFLAGS) -o $@ $(tresdns_OBJS) $(LDFLAGS) $(tresdns_LDADD)
