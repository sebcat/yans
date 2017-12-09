ACLOCAL_AMFLAGS = -I m4

AM_YFLAGS = -d
AM_LFLAGS = --header-file=tokens.h
AM_CFLAGS = -Wall -Werror -Wformat

noinst_PROGRAMS = genycl genlycl

genycl_SOURCES = yclgen.h tokens.h parser.y tokens.l genycl.c

genlycl_SOURCES = yclgen.h tokens.h parser.y tokens.l genlycl.c
