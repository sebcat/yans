rc_ddir = $(sysconfdir)/rc.d
dist_rc_d_SCRIPTS = etc.rc.d/ethd
CLEANFILES = $(dist_rc_d_SCRIPTS)
EXTRA_DIST = etc.rc.d/ethd.in


etc.rc.d/ethd: etc.rc.d/ethd.in
	sed -e "s,@BINDIR@,$(bindir)," \
		-e "s,@LOCALSTATEDIR@,$(localstatedir)," \
		 < etc.rc.d/ethd.in > etc.rc.d/ethd

