$(punycode_EGGLIB): $(punycode_EGG) $(punycode_EGGDEPS)
	$(CSC) -o $@ $(punycode_EGG) $(punycode_EGGDEPS)

$(ycl_EGGLIB): $(ycl_EGG) $(ycl_EGGDEPS)
	$(CSC) -o $@ $(ycl_EGG) $(ycl_EGGDEPS)
