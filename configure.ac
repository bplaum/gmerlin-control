AC_INIT([gmerlin-control], [1.0.0])
AC_CONFIG_SRCDIR([include/bgctrl.h])
AM_INIT_AUTOMAKE

AM_CONFIG_HEADER(include/config.h)

SAVE_CFLAGS=$CFLAGS
SAVE_LDFLAGS=$LDFLAGS
AC_DISABLE_STATIC
AC_LANG(C)
AC_PROG_LIBTOOL

CFLAGS=$SAVE_CFLAGS

dnl
dnl Required libraries
dnl

GMERLIN_REQUIRED="2.0.0"
PKG_CHECK_MODULES(GMERLIN, gmerlin >= $GMERLIN_REQUIRED, , AC_MSG_ERROR("gmerlin not found"))

AC_SUBST(GMERLIN_REQUIRED)

AC_CONFIG_FILES([Makefile \
include/Makefile \
lib/Makefile \
apps/Makefile \
apps/controlpanel/Makefile \
tests/Makefile \
])

AC_OUTPUT
