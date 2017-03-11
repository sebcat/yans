#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([yans], [1.0.0], [seb.cato@gmail.com])
AC_CANONICAL_TARGET
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([subdir-objects foreign -Wall -Werror])
AM_SILENT_RULES([yes])
AM_PROG_AR

LT_INIT

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h limits.h locale.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_FUNC_STRCOLL
AC_CHECK_FUNCS([atexit dup2 memchr memmove memset mkdir regcomp rmdir setlocale socket strcasecmp strchr strcspn strdup strerror strpbrk strrchr strspn strstr strtoul])

AC_CHECK_LIB([pcap], [pcap_create], [PCAPLIB=-lpcap],
  [AC_MSG_ERROR([libpcap missing])])
AC_SUBST([PCAPLIB])

AC_CHECK_LIB([event], [event_base_dispatch], [EVENTLIB=-levent],
  [AC_MSG_ERROR([libevent2 missing])])
AC_SUBST([EVENTLIB])

# Linux only: libcap
AC_CHECK_LIB([cap], [cap_set_flag], [CAPLIB=-lcap], [CAPLIB=])
AC_SUBST([CAPLIB])

AM_CONDITIONAL([FREEBSD_OPT], [
  case "$target_os" in freebsd*) true; ;;  *) false; ;; esac])

AC_CONFIG_FILES([Makefile 3rd_party/Makefile freebsd/Makefile])

AC_OUTPUT
