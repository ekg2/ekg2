dnl $Id$

AC_INIT(ekg/ekg.c)
AC_PREREQ(2.50)
AC_CONFIG_HEADERS(config.h)

AM_INIT_AUTOMAKE(ekg, CVS)

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_C_CONST

AC_LIBTOOL_DLOPEN
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

dnl  strlcat/strlcpy wiele systemów nie posiada
AC_CHECK_FUNCS(strlcat, , [COMPAT="$COMPAT strlcat.o"])
AC_CHECK_FUNCS(strlcpy, , [COMPAT="$COMPAT strlcpy.o"])

dnl  utimes do sprawdzania poczty
AC_CHECK_FUNCS(utimes)


AC_CONFIG_FILES([plugins/ioctld/Makefile])
AC_CONFIG_FILES([plugins/mail/Makefile])
AC_CONFIG_FILES([plugins/rc/Makefile])
AC_CONFIG_FILES([plugins/sms/Makefile])
AC_CONFIG_FILES([plugins/pcm/Makefile])
PLUGINS="mail ioctld rc sms pcm"

export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig"

AC_ARG_WITH(libgadu,
  [  --without-libgadu        Disable GG plugin])

if test "x$with_libgadu" != "xno"; then
	PKG_CHECK_MODULES(LIBGADU, libgadu,
	[
		AC_CONFIG_FILES([plugins/gg/Makefile])
		PLUGINS="$PLUGINS gg"
		AC_DEFINE(HAVE_LIBGADU, [], [Define if libgadu is found])
	])
fi
	
PKG_CHECK_MODULES(OPENSSL, openssl,
[
	AC_CONFIG_FILES([plugins/sim/Makefile])
	PLUGINS="$PLUGINS sim"
])

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

AC_CONFIG_FILES([Makefile ekg/Makefile compat/Makefile plugins/Makefile contrib/Makefile docs/Makefile])

PLUGINS_LIBS=""

for i in $PLUGINS; do
	PLUGINS_LIBS="$PLUGINS_LIBS \$(top_builddir)/plugins/$i/$i.la"
done

CFLAGS="$CFLAGS -Wall"

plugindir=$libdir/ekg/plugins
AC_SUBST(plugindir)

AC_OUTPUT

