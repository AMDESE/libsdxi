ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall

noinst_HEADERS = samples.h

context_SOURCES = context.c
memcopy_SOURCES = memcopy.c
uadd_SOURCES = uadd.c
write_imm_SOURCES = write-imm.c
repcopy_SOURCES = repcopy.c
nop_SOURCES = nop.c

noinst_PROGRAMS = context memcopy uadd write_imm repcopy nop
LDADD = ../src/libsdxi.la
