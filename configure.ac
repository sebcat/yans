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

AC_CHECK_LIB([pcap], [pcap_create], [PCAPLIB=-lpcap],
  [AC_MSG_ERROR([libpcap missing])])
AC_SUBST([PCAPLIB])

# Linux only: libcap
AC_CHECK_LIB([cap], [cap_set_flag], [CAPLIB=-lcap], [CAPLIB=])
AC_SUBST([CAPLIB])

AM_CONDITIONAL([FREEBSD_OPT], [
  case "$target_os" in freebsd*) true; ;;  *) false; ;; esac])

AC_CONFIG_FILES([Makefile freebsd/Makefile])

AC_OUTPUT
