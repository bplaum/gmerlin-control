plugindir=$(pkglibdir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/include

AM_CFLAGS = \
@GMERLIN_CFLAGS@ \
-DLOCALE_DIR=\"$(localedir)\" \
-DPLUGIN_DIR=\"$(plugindir)\" \
-DDOC_DIR=\"$(docdir)\" \
-DDATA_DIR=\"$(pkgdatadir)\"

AM_LDFLAGS = @GMERLIN_EXE_LDFLAGS@

bin_PROGRAMS = gmerlin-controlcenter

gmerlin_controlcenter_SOURCES = \
gmerlin-controlcenter.c \
controlcenter.c

gmerlin_controlcenter_LDADD = ../../lib/libgmerlin-control.la @GMERLIN_LIBS@ @GAVL_LIBS@

noinst_HEADERS = \
controlcenter.h


# EXTRA_DIST = gmerlin-remote.desktop
