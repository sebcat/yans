lib_ycl_SOURCES = lib/ycl/ycl.c
lib_ycl_HEADERS = lib/ycl/ycl.h
lib_ycl_CODEGEN = lib/ycl/ycl_msg.c
lib_ycl_GENSRCS = lib/ycl/ycl_msg.ycl
lib_ycl_OBJS = ${lib_ycl_SOURCES:.c=.o} ${lib_ycl_GENSRCS:.ycl=.o}

OBJS += ${lib_ycl_OBJS}
CODEGEN += ${lib_ycl_CODEGEN}
