plugindir=$(pkglibdir)/plugins

AM_CFLAGS = \
-DLOCALE_DIR=\"$(localedir)\" \
-DPLUGIN_DIR=\"$(plugindir)\" \
-DDOC_DIR=\"$(docdir)\" \
-DDATA_DIR=\"$(pkgdatadir)\"

AM_LDFLAGS = @GMERLIN_EXE_LDFLAGS@

AM_CPPFLAGS = -I$(top_srcdir)/include

bin_PROGRAMS = energenie shellyplug

energenie_LDADD = @GAVL_LIBS@ @GMERLIN_LIBS@ -ldl
shellyplug_LDADD = @GAVL_LIBS@ @GMERLIN_LIBS@ -ldl @JSON_LIBS@
