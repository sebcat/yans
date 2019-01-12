jansson_SOURCES = 3rd_party/jansson.c
jansson_HEADERS = 3rd_party/jansson.h
jansson_OBJS = ${jansson_SOURCES:.c=.o}

linenoise_SOURCES = 3rd_party/linenoise.c
linenoise_HEADERS = 3rd_party/linenoise.h
linenoise_OBJS = ${linenoise_SOURCES:.c=.o}

lua_SOURCES = 3rd_party/lua.c
lua_HEADERS = 3rd_party/lua.h
lua_OBJS = ${lua_SOURCES:.c=.o}

OBJS += $(jansson_OBJS) $(linenoise_OBJS) $(lua_OBJS)
