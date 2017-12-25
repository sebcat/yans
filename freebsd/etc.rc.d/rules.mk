freebsd/etc.rc.d/ethd: freebsd/etc.rc.d/ethd.in
	sed -e "s,@BINDIR@,$(BINDIR)," \
		-e "s,@LOCALSTATEDIR@,$(LOCALSTATEDIR)," \
		< freebsd/etc.rc.d/ethd.in > freebsd/etc.rc.d/ethd

freebsd/etc.rc.d/stored: freebsd/etc.rc.d/stored.in
	sed -e "s,@BINDIR@,$(BINDIR)," \
		-e "s,@LOCALSTATEDIR@,$(LOCALSTATEDIR)," \
		< freebsd/etc.rc.d/stored.in > freebsd/etc.rc.d/stored

