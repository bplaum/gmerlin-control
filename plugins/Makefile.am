## Process this file with automake to produce Makefile.in.

gmerlin_plugindir = @gmerlin_plugindir@

AM_CPPFLAGS = -I$(top_srcdir)/include

AM_CFLAGS = @GMERLIN_CFLAGS@ $(cdio_cflags) -DLOCALE_DIR=\"$(localedir)\"

AM_LDFLAGS = -avoid-version -module @GMERLIN_PLUGIN_LDFLAGS@

gmerlin_plugin_LTLIBRARIES = \
ctrl_shellyplug.la

ctrl_shellyplug_la_SOURCES = ctrl_shellyplug.c
ctrl_shellyplug_la_LIBADD = @GMERLIN_LIBS@ ../lib/libgmerlin-control.la

