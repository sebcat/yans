tools/linux/systemd/stored.service: tools/linux/systemd/stored.service.in
	sed -e "s,@BINDIR@,$(BINDIR)," \
		-e "s,@LOCALSTATEDIR@,$(LOCALSTATEDIR)," \
		-e "s,@DATAROOTDIR@,$(DATAROOTDIR)," \
		< $@.in > $@;
