ACLOCAL_AMFLAGS = -I m4

AUTOMAKE_OPTIONS = foreign
SUBDIRS = src docs samples

include_HEADERS = include/libsdxi.h

EXTRA_DIST = LICENSE

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libsdxi.pc

sdxiincludedir = $(includedir)/sdxi
sdxiinclude_HEADERS = \
	include/sdxi/types.h \
	include/sdxi/root.h \
	include/sdxi/util.h \
	include/sdxi/dev.h \
	include/sdxi/event.h \
	include/sdxi/key.h \
	include/sdxi/vendor.h \
	include/sdxi/cxt.h \
	include/sdxi/dp.h

sdxivendorincludedir = $(includedir)/sdxi/vendor
sdxivendorinclude_HEADERS = \
	include/sdxi/vendor/amd_qos.h

distclean-local:
	-rm -f \
		$(top_srcdir)/*.plist \
		$(top_srcdir)/libsdxi-*.tar.gz \
		$(top_builddir)/*.plist \
		$(top_builddir)/libsdxi-*.tar.gz
	-rm -rf \
		$(top_srcdir)/autom4te.cache \
		$(top_srcdir)/m4 \
		$(top_builddir)/samples/.deps \
		$(top_builddir)/src/.deps \
		$(top_builddir)/src/vendor/.deps
	-rm -f \
		$(top_srcdir)/INSTALL \
		$(top_srcdir)/Makefile.in \
		$(top_srcdir)/aclocal.m4 \
		$(top_srcdir)/ar-lib \
		$(top_srcdir)/compile \
		$(top_srcdir)/config.guess \
		$(top_srcdir)/config.sub \
		$(top_srcdir)/configure \
		$(top_srcdir)/depcomp \
		$(top_srcdir)/docs/Makefile.in \
		$(top_srcdir)/install-sh \
		$(top_srcdir)/ltmain.sh \
		$(top_srcdir)/missing \
		$(top_srcdir)/samples/Makefile.in \
		$(top_srcdir)/src/Makefile.in

check-local:
	@set -e; \
	tmpdir=$$(mktemp -d); \
	trap 'rm -rf "$$tmpdir"' EXIT INT TERM; \
	for h in $(include_HEADERS) $(sdxiinclude_HEADERS) $(sdxivendorinclude_HEADERS); do \
		inc=$${h#include/}; \
		obj=$$tmpdir/$$(basename "$$h").o; \
		printf '#include <%s>\nint main(void) { return 0; }\n' "$$inc" | \
			$(CC) $(CPPFLAGS) $(CFLAGS) -I$(top_srcdir)/include \
				-Wall -Wextra -Werror -x c -c -o "$$obj" -; \
	done
	@set -e; \
	nm -D --defined-only $(top_builddir)/src/.libs/libsdxi.so | \
		awk '($$3 != "" && $$3 !~ /^sdxi_/ && $$3 !~ /^LIBSDXI_/) { bad=1; print "unexpected exported symbol: " $$3 } END { exit bad }'
