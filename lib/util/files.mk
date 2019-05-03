lib_util_SOURCES = \
    lib/util/buf.c \
    lib/util/csv.c \
    lib/util/deduptbl.c \
    lib/util/eds.c \
    lib/util/flagset.c \
    lib/util/io.c \
    lib/util/idset.c \
    lib/util/idtbl.c \
    lib/util/lines.c \
    lib/util/netstring.c \
    lib/util/nullfd.c \
    lib/util/objalloc.c \
    lib/util/objtbl.c \
    lib/util/os.c \
    lib/util/prng.c \
    lib/util/reorder.c \
    lib/util/sandbox.c \
    lib/util/sha1.c \
    lib/util/sindex.c \
    lib/util/str.c \
    lib/util/sysinfo.c \
    lib/util/u8.c \
    lib/util/x509.c \
    lib/util/ylog.c \
    lib/util/zfile.c

lib_util_SOURCES_CC = lib/util/reset.cc

CXXFLAGS_lib_util_reset != pkg-config --cflags re2
LDFLAGS_lib_util_reset  != pkg-config --libs re2

lib_util_OBJS = ${lib_util_SOURCES:.c=.o} ${lib_util_SOURCES_CC:.cc=.o}

lib_util_csv_test_DEPS = lib/util/csv.c lib/util/buf.c
lib_util_csv_test_DEPSOBJS = ${lib_util_csv_test_DEPS:.c=.o}
lib_util_csv_test_SOURCES = lib/util/csv_test.c
lib_util_csv_test_OBJS = ${lib_util_csv_test_SOURCES:.c=.o}

lib_util_flagset_test_DEPS = lib/util/flagset.c
lib_util_flagset_test_DEPSOBJS = ${lib_util_flagset_test_DEPS:.c=.o}
lib_util_flagset_test_SOURCES = lib/util/flagset_test.c
lib_util_flagset_test_OBJS = ${lib_util_flagset_test_SOURCES:.c=.o}

lib_util_idtbl_test_DEPS = lib/util/idtbl.c
lib_util_idtbl_test_DEPSOBJS = ${lib_util_idtbl_test_DEPS:.c=.o}
lib_util_idtbl_test_SOURCES = lib/util/idtbl_test.c
lib_util_idtbl_test_OBJS = ${lib_util_idtbl_test_SOURCES:.c=.o}

lib_util_objtbl_test_DEPS = lib/util/objtbl.c lib/alloc/linfix.c
lib_util_objtbl_test_DEPSOBJS = ${lib_util_objtbl_test_DEPS:.c=.o}
lib_util_objtbl_test_SOURCES = lib/util/objtbl_test.c
lib_util_objtbl_test_OBJS = ${lib_util_objtbl_test_SOURCES:.c=.o}

lib_util_netstring_test_DEPS = lib/util/buf.c lib/util/netstring.c
lib_util_netstring_test_DEPSOBJS = ${lib_util_netstring_test_DEPS:.c=.o}
lib_util_netstring_test_SOURCES = lib/util/netstring_test.c
lib_util_netstring_test_OBJS = ${lib_util_netstring_test_SOURCES:.c=.o}

lib_util_os_test_DEPS = lib/util/os.c
lib_util_os_test_DEPSOBJS = ${lib_util_os_test_DEPS:.c=.o}
lib_util_os_test_SOURCES = lib/util/os_test.c
lib_util_os_test_OBJS = ${lib_util_os_test_SOURCES:.c=.o}

lib_util_u8_test_DEPS = lib/util/u8.c
lib_util_u8_test_DEPSOBJS = ${lib_util_u8_test_DEPS:.c=.o}
lib_util_u8_test_SOURCES = lib/util/u8_test.c
lib_util_u8_test_OBJS = ${lib_util_u8_test_SOURCES:.c=.o}

lib_util_str_test_DEPS = lib/util/str.c
lib_util_str_test_DEPSOBJS = ${lib_util_str_test_DEPS:.c=.o}
lib_util_str_test_SOURCES = lib/util/str_test.c
lib_util_str_test_OBJS = ${lib_util_str_test_SOURCES:.c=.o}

lib_util_idset_test_DEPS = lib/util/idset.c
lib_util_idset_test_DEPSOBJS = ${lib_util_idset_test_DEPS:.c=.o}
lib_util_idset_test_SOURCES = lib/util/idset_test.c
lib_util_idset_test_OBJS = ${lib_util_idset_test_SOURCES:.c=.o}

lib_util_reset_test_DEPS = lib/util/reset.c
lib_util_reset_test_DEPSOBJS = ${lib_util_reset_test_DEPS:.c=.o}
lib_util_reset_test_SOURCES = lib/util/reset_test.c
lib_util_reset_test_OBJS = ${lib_util_reset_test_SOURCES:.c=.o}


lib_util_CTESTSRCS = \
    $(lib_util_csv_test_SOURCES) \
    $(lib_util_flagset_test_SOURCES) \
    $(lib_util_idtbl_test_SOURCES) \
    $(lib_util_objtbl_test_SOURCES) \
    $(lib_util_netstring_test_SOURCES) \
    $(lib_util_os_test_SOURCES) \
    $(lib_util_u8_test_SOURCES) \
    $(lib_util_str_test_SOURCES) \
    $(lib_util_idset_test_SOURCES) \
    $(lib_util_reset_test_SOURCES)

lib_util_CTESTS = ${lib_util_CTESTSRCS:.c=}

OBJS += ${lib_util_OBJS} ${lib_util_CTESTSRCS:.c=.o}
CTESTS += ${lib_util_CTESTS}
