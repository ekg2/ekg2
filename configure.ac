AC_PREREQ([2.60])
AC_INIT([ekg2], [git])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.6 foreign dist-bzip2])

AC_PROG_CC_C99

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.16 gmodule-2.0])
LIBS="$LIBS $GLIB_LIBS"
CFLAGS="$CFLAGS $GLIB_CFLAGS"

AC_CHECK_HEADERS(regex.h)
AC_CHECK_FUNCS([flock getaddrinfo inet_ntop inet_pton inet_aton])

AC_CHECK_TYPE([socklen_t],,,[
#include <sys/types.h>
#include <sys/socket.h>
])

AC_CHECK_MEMBERS([struct kinfo_proc.ki_size],,,[
#include <sys/param.h>
#include <sys/user.h>
])

dnl compats -- XXX: get rid of them
AC_CHECK_FUNCS([scandir],,
	AC_MSG_ERROR([One of the required libc functions not found]))

AC_CONFIG_HEADERS([ekg2-config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
