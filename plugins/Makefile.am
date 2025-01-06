## Process this file with automake to produce Makefile.in.

gmerlin_plugindir = @gmerlin_plugindir@

AM_CPPFLAGS = -I$(top_srcdir)/include

AM_CFLAGS = @GMERLIN_CFLAGS@ $(cdio_cflags) -DLOCALE_DIR=\"$(localedir)\"

AM_LDFLAGS = -avoid-version -module @GMERLIN_PLUGIN_LDFLAGS@

if HAVE_PULSEAUDIO
pulse_plugins = ctrl_pulse.la
else
pulse_plugins =
endif

if HAVE_WIREPLUMBER
wireplumber_plugins = ctrl_wireplumber.la
else
wireplumber_plugins =
endif

gmerlin_plugin_LTLIBRARIES = \
ctrl_shellyplug.la \
ctrl_shellyplugplus.la \
ctrl_shellybulb_m.la \
ctrl_shellyht.la \
ctrl_command.la \
ctrl_energenie.la \
ctrl_marantz.la \
ctrl_sysinfo.la \
$(pulse_plugins) \
$(wireplumber_plugins)

ctrl_shellyplug_la_SOURCES = ctrl_shellyplug.c
ctrl_shellyplug_la_LIBADD = @GMERLIN_LIBS@ ../lib/libgmerlin-control.la

ctrl_shellyplugplus_la_SOURCES = ctrl_shellyplugplus.c
ctrl_shellyplugplus_la_LIBADD = @GMERLIN_LIBS@ ../lib/libgmerlin-control.la

ctrl_shellyht_la_SOURCES = ctrl_shellyht.c
ctrl_shellyht_la_LIBADD = @GMERLIN_LIBS@ ../lib/libgmerlin-control.la

ctrl_shellybulb_m_la_SOURCES = ctrl_shellybulb_m.c
ctrl_shellybulb_m_la_LIBADD = @GMERLIN_LIBS@ ../lib/libgmerlin-control.la

ctrl_marantz_la_SOURCES = ctrl_marantz.c
ctrl_marantz_la_LIBADD = @GMERLIN_LIBS@ ../lib/libgmerlin-control.la

ctrl_energenie_la_SOURCES = ctrl_energenie.c
ctrl_energenie_la_LIBADD = @GMERLIN_LIBS@ ../lib/libgmerlin-control.la

ctrl_command_la_SOURCES = ctrl_command.c
ctrl_command_la_LIBADD = @GMERLIN_LIBS@ ../lib/libgmerlin-control.la

ctrl_sysinfo_la_SOURCES = ctrl_sysinfo.c
ctrl_sysinfo_la_LIBADD = @GMERLIN_LIBS@ ../lib/libgmerlin-control.la

ctrl_pulse_la_SOURCES = ctrl_pulse.c
ctrl_pulse_la_LIBADD = @PULSEAUDIO_LIBS@ @GMERLIN_LIBS@ ../lib/libgmerlin-control.la

ctrl_wireplumber_la_CFLAGS = $(AM_CFLAGS) -DDATA_DIR=\"$(pkgdatadir)\"
ctrl_wireplumber_la_SOURCES = ctrl_wireplumber.c
ctrl_wireplumber_la_LIBADD = @GMERLIN_LIBS@ ../lib/libgmerlin-control.la

pkgdata_DATA = wireplumber_monitor.lua
dist_pkgdata_DATA = wireplumber_monitor.lua
