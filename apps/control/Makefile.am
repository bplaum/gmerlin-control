plugindir=$(pkglibdir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/include

AM_CFLAGS = \
@GMERLIN_CFLAGS@ \
-DLOCALE_DIR=\"$(localedir)\" \
-DPLUGIN_DIR=\"$(plugindir)\" \
-DDOC_DIR=\"$(docdir)\" \
-DDATA_DIR=\"$(pkgdatadir)\"

AM_LDFLAGS = @GMERLIN_EXE_LDFLAGS@

bin_PROGRAMS = gmerlin-control

gmerlin_control_SOURCES = \
gmerlin-control.c

gmerlin_control_LDADD = @GMERLIN_LIBS@ @GAVL_LIBS@ ../../lib/libgmerlin-control.la

