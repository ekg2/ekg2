dnl $Id$

AC_INIT(ekg/ekg.c)
AC_PREREQ(2.50)
AC_CONFIG_HEADERS(ekg2-config.h)

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
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)
AM_GNU_GETTEXT([external])

AC_SUBST(COMPAT)
AC_SUBST(PLUGINS)
AC_SUBST(PLUGINS_LIBS)

dnl FreeBSD
AC_NEED_STDINT_H

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

dnl inet_pton
AC_CHECK_FUNCS(inet_pton)

dnl  strlcat,strlcpy itp wiele systemów nie posiada
AC_CHECK_FUNCS(strlcat, , [COMPAT="$COMPAT strlcat.o"])
AC_CHECK_FUNCS(strlcpy, , [COMPAT="$COMPAT strlcpy.o"])
AC_CHECK_FUNCS(strndup, , [COMPAT="$COMPAT strndup.o"])
AC_CHECK_FUNCS(strlen, , [COMPAT="$COMPAT strlen.o"])
AC_CHECK_FUNCS(strnlen, , [COMPAT="$COMPAT strnlen.o"])
AC_CHECK_FUNCS(strfry, , [COMPAT="$COMPAT strfry.o"])

dnl  utimes do sprawdzania poczty
AC_CHECK_FUNCS(utimes)

dnl  mkstemp do zapisywania tokenów przy braku libjpeg
AC_CHECK_FUNCS(mkstemp)

AC_CONFIG_FILES([Makefile ekg/Makefile compat/Makefile plugins/Makefile contrib/Makefile docs/Makefile plugins/mail/Makefile plugins/logs/Makefile plugins/rc/Makefile plugins/sms/Makefile plugins/pcm/Makefile])
PLUGINS="logs mail rc sms pcm"

export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig"

mouse_lib="xterm only"
AC_ARG_WITH(gpm-mouse,
        [  --with-gpm-mouse         Compile with gpm mouse support (Linux only)])

case $host_os in
linux*)
    if test x$with_gpm_mouse != xno; then
        AC_CHECK_LIB(gpm, Gpm_GetEvent,
            [AC_DEFINE(HAVE_LIBGPM, 1,
                       [Define to enable gpm mouse support on Linux])
            mouse_lib="gpm and xterm"
            LIBGPM_LIBS="-lgpm"],
            [AC_MSG_WARN([libgpm is missing])
        ])
    fi
    ;;
esac

AC_ARG_WITH(ioctld,
  [  --without-ioctld        Disable ioctld plugin])

if test "x$with_ioctld" != "xno"; then
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

dnl
dnl  Resolver libgadu oparty na pthread
dnl

AC_ARG_WITH(pthread,
  [  --with-pthread          Use pthread in resolver])

if test "x$with_pthread" != "xno"; then
        ACX_PTHREAD(
        [
                dnl workaround dla pkconfiga
                if test "x$PTHREAD_CFLAGS" = "x-pthread"; then
                        PTHREAD_LIBS="$PTHREAD_LIBS -pthread"
                fi

                have_pthread=yes
        ], [
                AC_MSG_ERROR([Your system is not supporting pthreads])
        ])
fi

dnl
dnl  sprawdzanie pisowni w ncurses
dnl

AC_ARG_WITH(aspell,
  [  --with-aspell          Enable aspell support])

if test "x$with_aspell" != "xno"; then
       AC_CHECK_HEADERS(aspell.h,
        [
                have_aspell_includes=yes
        ])
        if test "x$have_aspell_includes" = "xyes"; then
            AC_CHECK_LIB(aspell, new_aspell_config,
            [
                have_aspell_libs=yes
            ])
            if test "x$have_aspell_libs" = "xyes"; then
            	AC_DEFINE(WITH_ASPELL, 1, [define if you want aspell support])
                ASPELL_LIBS="$ASPELL_LIBS -laspell"
		AC_SUBST(ASPELL_LIBS)
		have_aspell=yes
            else
                have_aspell=no
            fi
        else
            have_aspell=no
        fi
fi

dnl
dnl  Sprawdzamy libjpeg.so i <jpeglib.h>
dnl

AC_ARG_WITH(libjpeg,
        [  --without-libjpeg        Compile without JPEG token support])

if test "x$with_libjpeg" != "xno"; then
        AC_CHECK_LIB(jpeg, jpeg_start_decompress,
        [
                AC_CHECK_HEADERS(jpeglib.h,
                [
                        AC_DEFINE(HAVE_LIBJPEG, 1, [define if you have libjpeg])
                        LIBJPEG_LIBS="-ljpeg"
                        have_libjpeg=yes
                ])
        ])
fi

AC_CHECK_LIBGADU

if test "x$have_libgadu" = "xyes"; then
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

AM_PATH_LIBGNUTLS( 1.0.0,,AC_MSG_WARN([[
***
*** libgnutls was not found. You may want to get it from
*** ftp://ftp.gnutls.org/pub/gnutls/
]]))

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
 	echo " - ioctld:  enabled"
else
	echo " - ioctld:  disabled"
fi

if test "x$have_libgadu" = "xyes"; then
	echo " - libgadu: enabled"
else
	echo " - libgadu: disabled"
fi

if test "x$have_pthread" = "xyes"; then
	echo " - pthread: enabled"
else
	echo " - pthread: disabled"
fi

if test "x$have_openssl" = "xyes"; then
	echo " - openssl: enabled"
else
	echo " - openssl: disabled"
fi

if test "x$have_gnutls" = "xyes"; then
	echo " - gnutls:  enabled"
else
	echo " - gnutls:  disabled"
fi

if test "x$have_expat" = "xyes"; then
	echo " - expat:   enabled"
else
	echo " - expat:   disabled"
fi

if test "x$have_ncurses" = "xyes"; then
	echo " - ncurses: enabled"
else
	echo " - ncurses: disabled"
fi

if test "x$have_libgsm" = "xyes"; then
	echo " - libgsm:  enabled"
else
	echo " - libgsm:  disabled"
fi

if test "x$have_aspell" = "xyes"; then
        echo " - aspell:  enabled"
else
        echo " - aspell:  disabled"
fi

if test "x$have_libjpeg" = "xyes"; then
        echo " - libjpeg: enabled"
else
        echo " - libjpeg: disabled"
fi

echo " - mouse support in $mouse_lib"

