AC_PREREQ([2.60])
AC_INIT([ekg2], [git])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.6 foreign dist-bzip2])

AC_PROG_CC_C99

dnl 1) common requirements

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.16 gmodule-2.0])
LIBS="$LIBS $GLIB_LIBS"
CFLAGS="$CFLAGS $GLIB_CFLAGS"

AC_CONFIG_HEADERS([ekg2-config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
