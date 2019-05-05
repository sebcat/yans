$(lib_ycl_CODEGEN): $(lib_ycl_GENSRCS) apps/yclgen/genycl
	./apps/yclgen/genycl \
		lib/ycl/ycl_msg.h \
		lib/ycl/ycl_msg.c < lib/ycl/ycl_msg.ycl

$(lib_ycl_OBJS): $(lib_ycl_SOURCES) $(lib_ycl_CODEGEN)
