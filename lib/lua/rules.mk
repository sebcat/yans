$(lib_lua_CODEGEN): $(lib_lua_GENSRCS) tools/yclgen/genlycl
	./tools/yclgen/genlycl -f ./lib/ycl/ycl_msg.ycl \
		< ./lib/lua/ycl.c.in > ./lib/lua/ycl.c

$(lib_lua_OBJS): $(lib_lua_HEADERS)
