lib/lua/cgi.o: lib/lua/cgi.c lib/lua/cgi.h
lib/lua/eds.o: lib/lua/eds.c lib/lua/eds.h
lib/lua/file.o: lib/lua/file.c lib/lua/file.h
lib/lua/http.o: lib/lua/http.c lib/lua/http.h
lib/lua/json.o: lib/lua/json.c lib/lua/json.h
lib/lua/net.o: lib/lua/net.c lib/lua/net.h
lib/lua/opts.o: lib/lua/opts.c lib/lua/opts.h
lib/lua/util.o: lib/lua/util.c lib/lua/util.h
lib/lua/yans.o: lib/lua/yans.c lib/lua/net.h lib/lua/http.h lib/lua/json.h \
    lib/lua/cgi.h lib/lua/file.h lib/lua/opts.h lib/lua/ylog.h lib/lua/eds.h \
    lib/lua/ycl.h lib/lua/util.h lib/lua/yans.h
lib/lua/ycl.o: lib/lua/ycl.c lib/lua/ycl.h
lib/lua/ylog.o: lib/lua/ylog.c lib/lua/ylog.h

$(lib_lua_CODEGEN): $(lib_lua_GENSRCS) tools/yclgen/genlycl
	./tools/yclgen/genlycl -f ./lib/ycl/ycl_msg.ycl \
		< ./lib/lua/ycl.c.in > ./lib/lua/ycl.c

$(lib_lua_OBJS): $(lib_lua_HEADERS)
