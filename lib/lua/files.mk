lib_lua_SOURCES = \
    lib/lua/cgi.c \
    lib/lua/eds.c \
    lib/lua/file.c \
    lib/lua/http.c \
    lib/lua/json.c \
    lib/lua/net.c \
    lib/lua/opts.c \
    lib/lua/yans.c \
    lib/lua/ylog.c \
    lib/lua/util.c

lib_lua_HEADERS = \
    lib/lua/cgi.h \
    lib/lua/eds.h \
    lib/lua/file.h \
    lib/lua/http.h \
    lib/lua/json.h \
    lib/lua/net.h \
    lib/lua/opts.h \
    lib/lua/yans.h \
    lib/lua/ycl.h \
    lib/lua/ylog.h \
    lib/lua/util.h

lib_lua_CODEGEN = \
    lib/lua/ycl.c

lib_lua_GENSRCS = \
    lib/ycl/ycl_msg.ycl \
    lib/lua/ycl.c.in

lib_lua_OBJS = ${lib_lua_SOURCES:.c=.o} ${lib_lua_CODEGEN:.c=.o}

OBJS += $(lib_lua_OBJS)
CODEGEN += $(lib_lua_CODEGEN)
