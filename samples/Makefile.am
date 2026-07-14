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
cxt_intr_SOURCES = cxt_intr.c
opgroups_SOURCES = opgroups.c
crc_SOURCES = crc.c
xcopy_SOURCES = xcopy.c
cxt_err_SOURCES = cxt_err.c

noinst_PROGRAMS = context memcopy uadd write_imm repcopy nop cxt_intr opgroups crc xcopy cxt_err
LDADD = ../src/libsdxi.la
