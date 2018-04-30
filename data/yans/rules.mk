data/yans/ycfg.yans: data/yans/ycfg.yans.in
	sed -e "s,@BINDIR@,$(BINDIR)," \
		-e "s,@LOCALSTATEDIR@,$(LOCALSTATEDIR)," \
		-e "s,@DATAROOTDIR@,$(DATAROOTDIR)," \
		< $@.in > $@;
