lib_ycl_SOURCES = lib/ycl/ycl.c lib/ycl/yclcli.c lib/ycl/yclcli_store.c \
                  lib/ycl/yclcli_resolve.c lib/ycl/yclcli_kneg.c
lib_ycl_HEADERS = lib/ycl/ycl.h lib/ycl/yclcli.h lib/ycl/yclcli_store.h \
                  lib/ycl/yclcli_resolve.h lib/ycl/yclcli_kneg.h
lib_ycl_CODEGEN = lib/ycl/ycl_msg.c
lib_ycl_GENSRCS = lib/ycl/ycl_msg.ycl
lib_ycl_OBJS = ${lib_ycl_SOURCES:.c=.o} ${lib_ycl_GENSRCS:.ycl=.o}

OBJS += ${lib_ycl_OBJS}
CODEGEN += ${lib_ycl_CODEGEN}
