
punycode_EGG = lib/eggs/punycode.scm
punycode_EGGLIB = lib/eggs/punycode.so
punycode_EGGDEPS = lib/net/punycode.o lib/util/buf.o lib/util/u8.o

ycl_EGG = lib/eggs/ycl.scm
ycl_EGGLIB = lib/eggs/ycl.so
ycl_EGGDEPS = lib/util/io.o lib/util/buf.o lib/util/netstring.o lib/ycl/ycl.o

EGGLIBS += $(punycode_EGGLIB) $(ycl_EGGLIB)
