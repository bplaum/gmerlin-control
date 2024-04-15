AM_CPPFLAGS = -I$(top_srcdir)/include

AM_CFLAGS = -DLOCALE_DIR=\"$(localedir)\"

lib_LTLIBRARIES = libgmerlin-control.la

libgmerlin_control_la_SOURCES = \
control.c \
mqtt.c

libgmerlin_control_la_LIBADD = @MOSQUITTO_LIBS@ \
  @GAVL_LIBS@ \
  @GMERLIN_LIBS@


