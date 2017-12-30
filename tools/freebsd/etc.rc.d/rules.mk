$(GENERATED_RCFILES_FreeBSD): $(GENERATED_RCFILESIN_FreeBSD)
	for F in $(GENERATED_RCFILES_FreeBSD); do \
		sed -e "s,@BINDIR@,$(BINDIR)," \
			-e "s,@LOCALSTATEDIR@,$(LOCALSTATEDIR)," \
			-e "s,@DATAROOTDIR@,$(DATAROOTDIR)," \
			< $${F}.in > $${F}; \
	done
