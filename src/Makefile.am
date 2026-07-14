ACLOCAL_AMFLAGS = -I m4

#includedir = $(datadir)/include
#library_includedir = $(includedir)
#library_include_HEADERS = libhsdxi.h

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall -Wextra -fvisibility=hidden

noinst_HEADERS = \
	sdxi_abi.h \
	sdxi_dp.h \
	sdxi_internal.h \
	sdxi_uapi_internal.h \
	syscall.h

EXTRA_DIST = libsdxi.map

lib_LTLIBRARIES = libsdxi.la
libsdxi_la_SOURCES = \
	sdxi_abi.c \
	sdxi_util.c \
	sdxi_root.c \
	sdxi_dev.c \
	sdxi_keys.c \
	sdxi_cxt.c \
	sdxi_dp.c \
	vendor/amd_crypto.c \
	vendor/amd_qos.c

libsdxi_la_LDFLAGS = \
	-version-info 0:0:0 \
	-no-undefined \
	-Wl,--version-script=$(srcdir)/libsdxi.map
