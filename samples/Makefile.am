ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall

noinst_HEADERS = samples.h

context_SOURCES = context.c
memcopy_SOURCES = memcopy.c
uadd_SOURCES = uadd.c

noinst_PROGRAMS = context memcopy uadd
LDADD = ../src/libsdxi.la
