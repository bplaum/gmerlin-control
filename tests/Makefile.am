plugindir=$(pkglibdir)/plugins

AM_CFLAGS = \
-DLOCALE_DIR=\"$(localedir)\" \
-DPLUGIN_DIR=\"$(plugindir)\" \
-DDOC_DIR=\"$(docdir)\" \
-DDATA_DIR=\"$(pkgdatadir)\"

AM_LDFLAGS = @GMERLIN_EXE_LDFLAGS@

AM_CPPFLAGS = -I$(top_srcdir)/include

bin_PROGRAMS = energenie shellyplug

energenie_LDADD = $(top_builddir)/lib/libgmerlin.la -ldl
shellyplug_LDADD = $(top_builddir)/lib/libgmerlin.la -ldl  @JSON_LIBS@
