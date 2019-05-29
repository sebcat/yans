webfetch_DEPS     = lib/util/io.c lib/util/buf.c lib/util/netstring.c \
                    lib/util/zfile.c lib/net/tcpsrc.c \
                    lib/util/os.c lib/util/sandbox.c lib/ycl/opener.c \
                    lib/ycl/yclcli.c lib/ycl/yclcli_store.c lib/ycl/ycl.c \
                    lib/ycl/ycl_msg.c lib/match/component.c \
                    lib/util/csv.c lib/util/objtbl.c
webfetch_DEPSOBJS = ${webfetch_DEPS:.c=.o}  lib/match/reset.o
webfetch_SOURCES  = apps/webfetch/main.c apps/webfetch/fetch.c \
                    apps/webfetch/module.c \
                    apps/webfetch/modules/logger.c \
                    apps/webfetch/modules/writer.c \
                    apps/webfetch/modules/matcher.c
webfetch_HEADERS  = apps/webfetch/fetch.h apps/webfetch/module.h \
                    apps/webfetch/modules/logger.h \
                    apps/webfetch/modules/writer.h \
                    apps/webfetch/modules/matcher.h
webfetch_OBJS     = ${webfetch_SOURCES:.c=.o}
webfetch_BIN      = apps/webfetch/webfetch
webfetch_LDADD    = ${libcurl_LDFLAGS} ${re2_LDFLAGS} -lstdc++ -lz

CODEGEN += apps/webfetch/modules/matcher_httpheader.c
CODEGEN += apps/webfetch/modules/matcher_httpbody.c
OBJS    += $(webfetch_OBJS)
BINS    += $(webfetch_BIN)
