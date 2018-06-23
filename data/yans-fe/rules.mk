data/yans-fe/index.html: data/yans-fe/index.html.in
	sed -e "s,@PACKAGE_VERSION@,$(PACKAGE_VERSION)," \
		< $@.in > $@;

