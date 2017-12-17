$(lib_ycl_CODEGEN): $(lib_ycl_GENSRCS) tools/yclgen/genycl
	./tools/yclgen/genycl \
		lib/ycl/ycl_msg.h \
		lib/ycl/ycl_msg.c < lib/ycl/ycl_msg.ycl

$(lib_ycl_OBJS): $(lib_ycl_SOURCES) $(lib_ycl_CODEGEN)
