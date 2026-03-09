PACKAGE_STRING = "/src/vfs/gcs"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib/vfs \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

TESTS = \
	gcs_build_gs_url

check_PROGRAMS = $(TESTS)

gcs_build_gs_url_SOURCES = \
	gcs_build_gs_url.c
