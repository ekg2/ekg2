# Oron Peled (Sun Jun 30 2002)
# Taken from libglade.m4
# moidified by Piotr Kupisiewicz 

# a macro to get the libs/cflags for libxosd

dnl AM_CHECK_LIBXOSD([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test to see if libxosd is installed, and define LIBXOSD_CFLAGS, LIBXOSDLIBS
dnl
AC_DEFUN([AM_CHECK_LIBXOSD],
[dnl
dnl Get the cflags and libraries from the xosd-config script
dnl
AC_ARG_WITH(xosd-config,
AC_HELP_STRING([--with-xosd-config=LIBXOSD_CONFIG],[Location of xosd-config]),
LIBXOSD_CONFIG="$withval")

AC_PATH_PROG(LIBXOSD_CONFIG, xosd-config, no)
AC_MSG_CHECKING(for libxosd)
if test "$LIBXOSD_CONFIG" = "no"; then
  AC_MSG_RESULT(no)
  ifelse([$2], , :, [$2])
else
  have_libxosd="yes"
  LIBXOSD_CFLAGS=`$LIBXOSD_CONFIG --cflags`
  LIBXOSD_LIBS=`$LIBXOSD_CONFIG --libs`
  AC_MSG_RESULT(yes)
  ifelse([$1], , :, [$1])
fi
AC_SUBST(LIBXOSD_CFLAGS)
AC_SUBST(LIBXOSD_LIBS)
])
