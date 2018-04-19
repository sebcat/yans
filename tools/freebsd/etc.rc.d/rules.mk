
tools/freebsd/etc.rc.d/ethd: tools/freebsd/etc.rc.d/ethd.in
	sed -e "s,@BINDIR@,$(BINDIR)," \
		-e "s,@LOCALSTATEDIR@,$(LOCALSTATEDIR)," \
		-e "s,@DATAROOTDIR@,$(DATAROOTDIR)," \
		< $@.in > $@;

tools/freebsd/etc.rc.d/stored: tools/freebsd/etc.rc.d/stored.in
	sed -e "s,@BINDIR@,$(BINDIR)," \
		-e "s,@LOCALSTATEDIR@,$(LOCALSTATEDIR)," \
		-e "s,@DATAROOTDIR@,$(DATAROOTDIR)," \
		< $@.in > $@;

tools/freebsd/etc.rc.d/clid: tools/freebsd/etc.rc.d/clid.in
	sed -e "s,@BINDIR@,$(BINDIR)," \
		-e "s,@LOCALSTATEDIR@,$(LOCALSTATEDIR)," \
		-e "s,@DATAROOTDIR@,$(DATAROOTDIR)," \
		< $@.in > $@;

tools/freebsd/etc.rc.d/knegd: tools/freebsd/etc.rc.d/knegd.in
	sed -e "s,@BINDIR@,$(BINDIR)," \
		-e "s,@LOCALSTATEDIR@,$(LOCALSTATEDIR)," \
		-e "s,@DATAROOTDIR@,$(DATAROOTDIR)," \
		< $@.in > $@;

tools/freebsd/etc.rc.d/sc2: tools/freebsd/etc.rc.d/sc2.in
	sed -e "s,@BINDIR@,$(BINDIR)," \
		-e "s,@LOCALSTATEDIR@,$(LOCALSTATEDIR)," \
		-e "s,@DATAROOTDIR@,$(DATAROOTDIR)," \
		< $@.in > $@;

