ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall

noinst_HEADERS = samples.h

memcopy_SOURCES = memcopy.c
repcopy_SOURCES = repcopy.c
xcopy_SOURCES = xcopy.c
nop_SOURCES = nop.c
kill_cxt_SOURCES = kill_cxt.c
cxt_err_SOURCES = cxt_err.c
uadd_SOURCES = uadd.c
write_imm_SOURCES = write-imm.c
context_SOURCES = context.c
crc_SOURCES = crc.c
cxt_intr_SOURCES = cxt_intr.c
opgroups_SOURCES = opgroups.c

noinst_PROGRAMS = memcopy repcopy xcopy nop kill_cxt cxt_err uadd write_imm context crc cxt_intr opgroups
LDADD = ../src/libsdxi.la
