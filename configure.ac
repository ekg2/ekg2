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
AC_SUBST(LIBLTDL)
AC_SUBST(LTDLINCL)
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

dnl It's obsolete in current versions of gettext but it's needed in older
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.11.5)

ALL_LINGUAS="pl"

dnl little or big endian ? 
AC_C_BIGENDIAN

AC_SUBST(COMPAT)
AC_SUBST(PLUGINS)
AC_SUBST(PLUGINS_LIBS)

dnl FreeBSD
AC_NEED_STDINT_H

dnl  SunOS
AC_CHECK_FUNC(gethostbyname, [], [AC_CHECK_LIB(nsl, gethostbyname, LIBS="$LIBS -lnsl")])
AC_CHECK_FUNC(socket, [], [AC_CHECK_LIB(socket, socket, LIBS="$LIBS -lsocket")])

dnl  BeOS
AC_CHECK_FUNC(inet_addr, [], [AC_CHECK_LIB(bind, __inet_addr, LIBS="$LIBS -lbind")])

dnl  sprawdzamy, czy system ma getopt_long
AC_CHECK_FUNC(getopt_long, , [COMPAT="$COMPAT getopt.o getopt1.o"])

dnl  na SunOSach nie ma scandir()
AC_CHECK_FUNCS(scandir, , [COMPAT="$COMPAT scandir.o"])

dnl  setenv dla pythona
AC_CHECK_FUNCS(setenv)

dnl inet_pton
AC_CHECK_FUNCS(inet_pton)
AC_CHECK_FUNCS(inet_ntop)
AC_CHECK_FUNCS(getaddrinfo)

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

AC_CONFIG_SUBDIRS([libltdl])

AC_CONFIG_FILES([Makefile ekg/Makefile compat/Makefile plugins/Makefile contrib/Makefile docs/Makefile plugins/mail/Makefile plugins/logs/Makefile plugins/rc/Makefile plugins/sms/Makefile plugins/pcm/Makefile plugins/irc/Makefile po/Makefile.in ])
PLUGINS="logs mail rc sms pcm irc"

export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig"

AC_CHECK_PROG([PKGCONFIG], [pkg-config], [pkg-config], [no])

mouse_lib="xterm only"
AC_ARG_WITH(gpm-mouse,
        [  --without-gpm-mouse         Compile without gpm mouse support (Linux only)])

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
  [  --without-pthread          Don't use pthread in resolver])

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
dnl aspell dictionary
dnl

AC_ARG_WITH(aspell,
  [  --without-aspell          Disable aspell support])

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

AC_ARG_WITH(libgadu,
        [  --without-libgadu        Compile without libgadu (GG plugin)])

if test "x$with_libgadu" != "xno"; then
	AC_CHECK_LIBGADU
	
	if test "x$have_libgadu" = "xyes"; then
		AC_CONFIG_FILES([plugins/gg/Makefile])
		PLUGINS="$PLUGINS gg"
	fi
fi

AC_ARG_WITH(openssl,
        [  --without-openssl        Compile without OpenSSL support])

if test "x$with_openssl" != "xno"; then
	AC_CHECK_OPENSSL

	if test "x$have_openssl" = "xyes"; then
		AC_CONFIG_FILES([plugins/sim/Makefile])
		PLUGINS="$PLUGINS sim"
	fi
fi

AC_ARG_WITH(expat,
        [  --without-expat        Compile without expat (jabber plugin)])

if test "x$with_expat" != "xno"; then
	AC_CHECK_EXPAT

	if test "x$have_expat" = "xyes"; then
		AC_CONFIG_FILES([plugins/jabber/Makefile])
		PLUGINS="$PLUGINS jabber"
	fi
fi

AC_ARG_WITH(ncurses,
        [  --without-ncurses        Compile without ncurses support (ncurses plugin) - program won't work])

if test "x$with_ncurses" != "xno"; then
	AC_CHECK_NCURSES

	if test "x$have_ncurses" = "xyes"; then
		AC_CONFIG_FILES([plugins/ncurses/Makefile])
		PLUGINS="$PLUGINS ncurses"
		AC_CHECK_HEADERS(curses.h ncurses.h ncurses/ncurses.h)
	fi
fi

AC_ARG_WITH(libgsm,
        [  --without-libgsm        Compile without libgsm support (gsm plugin)])


if test "x$with_libgsm" != "xno"; then
	AC_CHECK_LIBGSM

	if test "x$have_libgsm" = "xyes"; then
		AC_CONFIG_FILES([plugins/gsm/Makefile])
		PLUGINS="$PLUGINS gsm"
	fi
fi


AC_ARG_WITH(libxosd,
        [  --without-libxosd        Compile without libxosd support (xosd plugin)])

if test "x$with_libxosd" != "xno"; then
	AM_CHECK_LIBXOSD

	if test "x$have_libxosd" = "xyes"; then
		AC_CONFIG_FILES([plugins/xosd/Makefile])
		PLUGINS="$PLUGINS xosd"
	fi
fi

AC_ARG_WITH(perl,
        [  --without-perl        Compile without perl support (perl plugin)])

if test "x$with_perl" != "xno"; then
	AM_CHECK_PERL

	if test "x$have_perl" = "xyes"; then
		AC_CONFIG_FILES([plugins/perl/Makefile])
		AC_CONFIG_FILES([plugins/perl/SCRIPTS/Makefile])
		PLUGINS="$PLUGINS perl"
	fi
fi


AC_ARG_WITH(python,
        [  --without-python        Compile without python support (python plugin)])

if test "x$with_python" != "xno"; then
	AM_CHECK_PYTHON

	if test "x$have_python" = "xyes"; then
		AC_CONFIG_FILES([plugins/python/Makefile])
		PLUGINS="$PLUGINS python"
	fi
fi

AC_ARG_WITH(sqlite3,
        [  --without-sqlite3        Compile without sqlite3 support (sqlite plugin)])

if test "x$with_sqlite3" != "xno"; then
	AC_CHECK_SQLITE3
	if test "x$have_sqlite3" = "xyes"; then
		AC_DEFINE(HAVE_SQLITE3, 1, [define if you have libsqlite3])
	fi
fi

AC_ARG_WITH(sqlite,
        [  --without-sqlite        Compile without sqlite support (sqlite plugin)])

if test "x$have_sqlite3" != "xyes"; then
	if test "x$with_sqlite" != "xno"; then
		AC_CHECK_SQLITE
	fi	
fi	
if test "x$have_sqlite" = "xyes" || test "x$have_sqlite3" = "xyes" ; then
	AC_CONFIG_FILES([plugins/logsqlite/Makefile])
	PLUGINS="$PLUGINS logsqlite"
fi

AC_ARG_WITH(libgnutls,
        [  --without-libgnutls        Compile without libgnutls support (TLS in jabber)])

if test "x$with_libgnutls" != "xno"; then
	AM_PATH_LIBGNUTLS( 1.0.0,,AC_MSG_WARN([[
***
*** libgnutls was not found. You may want to get it from
*** ftp://ftp.gnutls.org/pub/gnutls/
	]]))
fi

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
 	echo " - ioctld:    enabled"
else
	echo " - ioctld:    disabled"
fi

if test "x$have_libgadu" = "xyes"; then
	echo " - libgadu:   enabled"
else
	echo " - libgadu:   disabled"
fi

if test "x$have_pthread" = "xyes"; then
	echo " - pthread:   enabled"
else
	echo " - pthread:   disabled"
fi

if test "x$have_openssl" = "xyes"; then
	echo " - openssl:   enabled"
else
	echo " - openssl:   disabled"
fi

if test "x$have_gnutls" = "xyes"; then
	echo " - gnutls:    enabled"
else
	echo " - gnutls:    disabled"
fi

if test "x$have_expat" = "xyes"; then
	echo " - expat:     enabled"
else
	echo " - expat:     disabled"
fi

if test "x$have_ncurses" = "xyes"; then
	echo " - ncurses:   enabled"
else
	echo " - ncurses:   disabled"
fi

if test "x$have_libgsm" = "xyes"; then
	echo " - libgsm:    enabled"
else
	echo " - libgsm:    disabled"
fi

if test "x$have_perl" = "xyes"; then
	echo " - perl:    enabled"
else
	echo " - perl:    disabled"
fi

if test "x$have_python" = "xyes"; then
	echo " - python:    enabled"
else
	echo " - python:    disabled"
fi

if test "x$have_libxosd" = "xyes"; then
	echo " - libxosd:   enabled"
else
	echo " - libxosd:   disabled"
fi

if test "x$have_sqlite" = "xyes"; then
	echo " - logsqlite: enabled"
elif test "x$have_sqlite3" = "xyes"; then
	echo " - logsqlite: enabled (using sqlite3)"
else	
	echo " - logsqlite: disabled"
fi

if test "x$have_aspell" = "xyes"; then
        echo " - aspell:    enabled"
else
        echo " - aspell:    disabled"
fi

if test "x$have_libjpeg" = "xyes"; then
        echo " - libjpeg:   enabled"
else
        echo " - libjpeg:   disabled"
fi

echo " - mouse support in $mouse_lib"

