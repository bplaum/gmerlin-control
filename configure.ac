AC_INIT([gmerlin-control], [1.0.0])
AC_CONFIG_SRCDIR([include/bgctrl.h])
AM_INIT_AUTOMAKE

AM_CONFIG_HEADER(include/config.h)

AC_CONFIG_MACRO_DIRS([m4])

SAVE_CFLAGS=$CFLAGS
SAVE_LDFLAGS=$LDFLAGS
AC_DISABLE_STATIC
AC_LANG(C)
AC_PROG_LIBTOOL

CFLAGS=$SAVE_CFLAGS

LQT_OPT_CFLAGS(["-O3 -fomit-frame-pointer -ffast-math"])

CFLAGS="$CFLAGS -D_REENTRANT -D_FILE_OFFSET_BITS=64 $OPT_CFLAGS"

LQT_TRY_CFLAGS(-Wall, CFLAGS="$CFLAGS -Wall", )
LQT_TRY_CFLAGS(-Wmissing-declarations, CFLAGS="$CFLAGS -Wmissing-declarations", )
LQT_TRY_CFLAGS(-Wdeclaration-after-statement, CFLAGS="$CFLAGS -Wdeclaration-after-statement")


dnl
dnl Linker flags
dnl

GMERLIN_CHECK_LDFLAGS

dnl
dnl Required libraries
dnl

GAVL_REQUIRED="2.0.0"
PKG_CHECK_MODULES(GAVL, gavl >= $GAVL_REQUIRED, , AC_MSG_ERROR("gavl not found"))
AC_SUBST(GAVL_REQUIRED)

GMERLIN_REQUIRED="2.0.0"
PKG_CHECK_MODULES(GMERLIN, gmerlin >= $GMERLIN_REQUIRED, , AC_MSG_ERROR("gmerlin not found"))

AC_SUBST(GMERLIN_REQUIRED)
gmerlin_plugindir='$(libdir)/gmerlin/plugins'

dnl LDFLAGS for plugins
GMERLIN_PLUGIN_LDFLAGS="-export-symbols "`pkg-config --variable=prefix gmerlin`"/share/gmerlin/plugin.sym $SAVE_LDFLAGS"

AC_SUBST(GMERLIN_PLUGIN_LDFLAGS)
AC_SUBST(gmerlin_plugindir)

JSON_REQUIRED="0.11.0"
PKG_CHECK_MODULES(JSON, json-c >= $JSON_REQUIRED, , AC_MSG_ERROR("json not found"))
AC_SUBST(JSON_REQUIRED)

CFLAGS="$CFLAGS $GMERLIN_CFLAGS $GAVL_CFLAGS"

AC_CONFIG_FILES([Makefile \
include/Makefile \
lib/Makefile \
m4/Makefile \
apps/Makefile \
apps/controlpanel/Makefile \
apps/control/Makefile \
plugins/Makefile \
tests/Makefile \
web/Makefile \
web/js/Makefile \
web/css/Makefile \
])

AC_OUTPUT
