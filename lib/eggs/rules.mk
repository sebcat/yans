$(punycode_EGGLIB): $(punycode_EGG) $(punycode_EGGDEPS)
	$(CSC) -o $@ $(punycode_EGG) $(punycode_EGGDEPS)

$(ycl_EGG): $(ycl_EGG).in lib/ycl/ycl_msg.c lib/ycl/ycl_msg.ycl tools/yclgen/gensycl
	./tools/yclgen/gensycl \
		-f lib/ycl/ycl_msg.ycl < $(ycl_EGG).in > $(ycl_EGG)

$(ycl_EGGLIB): $(ycl_EGG) $(ycl_EGGDEPS)
	$(CSC) -o $@ $(ycl_EGG) $(ycl_EGGDEPS)
