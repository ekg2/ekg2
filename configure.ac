dnl $Id$

AC_INIT(ekg/ekg.c)
AC_PREREQ(2.50)
AC_CONFIG_HEADERS(config.h)

AM_INIT_AUTOMAKE(ekg2, CVS)

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_C_CONST

AC_LIBTOOL_DLOPEN
AC_LIBLTDL_CONVENIENCE
AC_CONFIG_SUBDIRS([libltdl])
AC_SUBST(LIBLTDL)
AC_SUBST(LTDLINCL)
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

AC_SUBST(COMPAT)
AC_SUBST(PLUGINS)
AC_SUBST(PLUGINS_LIBS)

dnl  SunOS
AC_CHECK_LIB(nsl, t_accept, LIBS="$LIBS -lnsl")
AC_CHECK_LIB(socket, socket, LIBS="$LIBS -lsocket")

dnl  BeOS
AC_CHECK_LIB(bind, __inet_addr, LIBS="$LIBS -lbind")

dnl  sprawdzamy, czy system ma getopt_long
AC_CHECK_FUNC(getopt_long, , [COMPAT="$COMPAT getopt.o getopt1.o"])

dnl  na SunOSach nie ma scandir()
AC_CHECK_FUNCS(scandir, , [COMPAT="$COMPAT scandir.o"])

dnl  setenv dla pythona
AC_CHECK_FUNCS(setenv)

dnl  strlcat,strlcpy itp wiele systemów nie posiada
AC_CHECK_FUNCS(strlcat, , [COMPAT="$COMPAT strlcat.o"])
AC_CHECK_FUNCS(strlcpy, , [COMPAT="$COMPAT strlcpy.o"])
AC_CHECK_FUNCS(strndup, , [COMPAT="$COMPAT strndup.o"])
AC_CHECK_FUNCS(strlen, , [COMPAT="$COMPAT strlen.o"])
AC_CHECK_FUNCS(strnlen, , [COMPAT="$COMPAT strnlen.o"])
AC_CHECK_FUNCS(strfry, , [COMPAT="$COMPAT strfry.o"])

dnl  utimes do sprawdzania poczty
AC_CHECK_FUNCS(utimes)


AC_CONFIG_FILES([plugins/mail/Makefile])
AC_CONFIG_FILES([plugins/rc/Makefile])
AC_CONFIG_FILES([plugins/sms/Makefile])
AC_CONFIG_FILES([plugins/pcm/Makefile])
PLUGINS="mail rc sms pcm"

export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig"

AC_ARG_WITH(ioctld,
  [  --without-ioctld        Disable ioctld plugin])

if test "x$without_ioctld" != "xyes"; then
        AC_CHECK_HEADERS(linux/kd.h,
	[
		have_ioctld_includes=yes
	], [
	        AC_CHECK_HEADERS(sys/kbio.h,
	        [
		        have_ioctld_includes=yes
		])
	])
        if test "x$have_ioctld_includes" = "xyes"; then
		AC_CONFIG_FILES([plugins/ioctld/Makefile])
		PLUGINS="$PLUGINS ioctld"
	fi
fi

AC_CHECK_LIBGADU

if test "x&have_libgadu" = "xyes"; then
	AC_CONFIG_FILES([plugins/gg/Makefile])
	PLUGINS="$PLUGINS gg"
fi

AC_CHECK_OPENSSL

if test "x$have_openssl" = "xyes"; then
	AC_CONFIG_FILES([plugins/sim/Makefile])
	PLUGINS="$PLUGINS sim"
fi

AC_CHECK_EXPAT

if test "x$have_expat" = "xyes"; then
	AC_CONFIG_FILES([plugins/jabber/Makefile])
	PLUGINS="$PLUGINS jabber"
fi

AC_CHECK_NCURSES

if test "x$have_ncurses" = "xyes"; then
	AC_CONFIG_FILES([plugins/ncurses/Makefile])
	PLUGINS="$PLUGINS ncurses"
	AC_CHECK_HEADERS(curses.h ncurses.h ncurses/ncurses.h)
fi

AC_CHECK_LIBGSM

if test "x$have_libgsm" = "xyes"; then
	AC_CONFIG_FILES([plugins/gsm/Makefile])
	PLUGINS="$PLUGINS gsm"
fi

AC_CONFIG_FILES([Makefile ekg/Makefile compat/Makefile plugins/Makefile contrib/Makefile docs/Makefile m4/Makefile])

PLUGINS_LIBS=""

for i in $PLUGINS; do
	PLUGINS_LIBS="$PLUGINS_LIBS \$(top_builddir)/plugins/$i/$i.la"
done

CFLAGS="$CFLAGS -Wall"

plugindir=$libdir/ekg2/plugins
AC_SUBST(plugindir)

AC_OUTPUT

echo
echo "configured options:"

if test "x$have_ioctld_includes" = "xyes"; then
 	echo " - ioctld: enabled"
else
	echo " - ioctld: disabled"
fi

if test "x&have_libgadu" = "xyes"; then
	echo " - libgadu: enabled"
else
	echo " - libgadu: disabled"
fi

if test "x$have_openssl" = "xyes"; then
	echo " - openssl: enabled"
else
	echo " - openssl: disabled"
fi

if test "x$have_expat" = "xyes"; then
	echo " - expat: enabled"
else
	echo " - expat: disabled"
fi

if test "x$have_ncurses" = "xyes"; then
	echo " - ncurses: enabled"
else
	echo " - ncurses: disabled"
fi

if test "x$have_libgsm" = "xyes"; then
	echo " - libgsm: enabled"
else
	echo " - libgsm: disabled"
fi

