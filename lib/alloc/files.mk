lib_alloc_SOURCES = lib/alloc/linfix.c lib/alloc/linvar.c
lib_alloc_HEADERS =
lib_alloc_OBJS = ${lib_alloc_SOURCES:.c=.o}

OBJS += $(lib_alloc_OBJS)
