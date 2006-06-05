dnl $Id$

AC_INIT(ekg/ekg.c)
AC_PREREQ(2.50)
AC_CONFIG_HEADERS(ekg2-config.h)

AM_INIT_AUTOMAKE(ekg2, CVS)


AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_C_CONST

dnl sprawdzic czy sa dobre usuniete flagi jak cos to dodac/ zmienic/ usunac/ etc.. /me nie wie.

AC_LIBTOOL_DLOPEN
dnl AC_LIBLTDL_CONVENIENCE
dnl AC_SUBST(LIBLTDL)
dnl AC_SUBST(LTDLINCL)

AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

dnl It's obsolete in current versions of gettext but it's needed in older
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.14.3])

ALL_LINGUAS="pl"

dnl little or big endian ? 
AC_C_BIGENDIAN

AC_SUBST(COMPAT)
AC_SUBST(DLLIB)
AC_SUBST(PLUGINS)
AC_SUBST(PLUGINS_LIBS)

dnl FreeBSD
AC_NEED_STDINT_H
AC_CHECK_FUNC(kvm_openfiles, [], [AC_CHECK_LIB(kvm, kvm_openfiles, LIBS="$LIBS -lkvm")])

dnl  SunOS
AC_CHECK_FUNC(gethostbyname, [], [AC_CHECK_LIB(nsl, gethostbyname, LIBS="$LIBS -lnsl")])
AC_CHECK_FUNC(socket, [], [AC_CHECK_LIB(socket, socket, LIBS="$LIBS -lsocket")])
AC_CHECK_FUNC(sched_yield, [], [AC_CHECK_LIB(rt, sched_yield, LIBS="$LIBS -lrt")])

dnl  BeOS
AC_CHECK_FUNC(inet_addr, [], [AC_CHECK_LIB(bind, __inet_addr, LIBS="$LIBS -lbind")])

dnl windows
AC_CHECK_LIB(wsock32, main, LIBS="$LIBS -lwsock32")

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

CFLAGS="$CFLAGS -Wall"

AC_PROG_C_STD

if test "x$C_STD" = "xyes"; then
	CFLAGS="$CFLAGS -std=c99"
fi

dnl  mkstemp do zapisywania tokenów przy braku libjpeg
AC_CHECK_FUNCS(mkstemp)
dnl to chyba tak ma byc.
AC_CHECK_MEMBERS([struct kinfo_proc.ki_size],,,[
#include <sys/param.h>
#include <sys/user.h>
])

dnl AC_CONFIG_SUBDIRS([libltdl])
AC_CHECK_FUNC(dlopen, [], [AC_CHECK_LIB(dl, dlopen, DLLIB="$DLLIB -ldl")])

AC_CONFIG_FILES([Makefile ekg/Makefile compat/Makefile plugins/Makefile contrib/Makefile docs/Makefile plugins/mail/Makefile plugins/logs/Makefile plugins/rc/Makefile plugins/sms/Makefile plugins/pcm/Makefile plugins/irc/Makefile po/Makefile.in ])
AC_CONFIG_FILES([ekg/ekg2-config])
PLUGINS="logs mail rc sms pcm irc"

export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig"

AC_CHECK_PROG([PKGCONFIG], [pkg-config], [pkg-config], [no])

mouse_lib="xterm only"
AC_ARG_WITH(gpm-mouse,
        [  --without-gpm-mouse         Compile without gpm mouse support (Linux only)])

case $host_os in
linux*)
    if test x$with_gpm_mouse != no; then
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
                AC_MSG_WARN([Your system is not supporting pthreads])
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
dnl  Checking libungif.so & <gif_lib.h> or
dnl  Checking libgif.so   & <gif_lib.h>
dnl

AC_ARG_WITH(gif,	[  --without-gif         Compile without GIF token support])
AC_ARG_WITH(libgif,	[  --without-libgif      Compile without libgif token support])
AC_ARG_WITH(libungif,	[  --without-libungif    Compile without libungif token support])


if test "x$with_gif" != "xno"; then
	if test "x$with_libgif" != "xno"; then
		AC_CHECK_LIB(gif, DGifSlurp, [
			AC_CHECK_HEADERS(gif_lib.h, [
				AC_DEFINE(HAVE_LIBGIF, 1, [define if you have libgif])
				AC_SUBST(GG_TOKENGIF_LIBS)
				GG_TOKENGIF_LIBS="-lgif"
				have_gif=yes
			])
		])
	fi
	if test "x$with_libungif" != "xno"; then
	        AC_CHECK_LIB(ungif, DGifSlurp,
        	[
	                AC_CHECK_HEADERS(gif_lib.h,
        	        [
	                        AC_DEFINE(HAVE_LIBUNGIF, 1, [define if you have libungif])
				AC_SUBST(GG_TOKENGIF_LIBS)
	                        GG_TOKENGIF_LIBS="-lungif"
        	                have_gif=yes
	                ])
	        ])
	fi
fi


dnl
dnl  Checking libjpeg.so i <jpeglib.h>
dnl

AC_ARG_WITH(libjpeg,
        [  --without-libjpeg        Compile without JPEG token support])

if test "x$with_libjpeg" != "xno"; then
        AC_CHECK_LIB(jpeg, jpeg_start_decompress,
        [
                AC_CHECK_HEADERS(jpeglib.h,
                [
                        AC_DEFINE(HAVE_LIBJPEG, 1, [define if you have libjpeg])
			AC_SUBST([LIBJPEG_LIBS])
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

AC_ARG_ENABLE([unicode],
    AC_HELP_STRING([--enable-unicode], [Enable unicode (EXPERIMENTAL)]),
    [
        if test "x$enable_unicode" = "xyes"; then
            AC_DEFINE([USE_UNICODE], 1, [Use unicode (wide character strings)])
        fi
    ]
)

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


AC_ARG_WITH(gtk,
	[  --without-gtk        Compile without gtk2 support (gtk plugin)])


if test "x$with_gtk" != "xno"; then
	AC_CHECK_GTK2([], [have_gtk="yes"], [have_gtk=x"no"], []) 
	if test "x$have_gtk" = "xyes"; then
		AC_CONFIG_FILES([plugins/gtk/Makefile])
		PLUGINS="$PLUGINS gtk"
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

plugindir=$libdir/ekg2/plugins
AC_SUBST(plugindir)

echo
echo "configured options:"

if test "x$have_ioctld_includes" = "xyes"; then
 	echo " - ioctld:    enabled"
else
	echo " - ioctld:    disabled"
fi

if test "x$have_libgadu" = "xyes"; then
	echo " - libgadu:   enabled"
	echo -n " - GG Tokens: saving to file"

	if test "x$have_libjpeg" = "xyes"; then
	        echo -n ", JPEG OCR (using: -ljpeg)"
	fi

	if test "x$have_gif" = "xyes"; then
		echo -n ", GIF OCR (using: $GG_TOKENGIF_LIBS)"
	fi
	echo ""
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
	echo " - perl:      enabled"
else
	echo " - perl:      disabled"
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

if test "x$enable_unicode" = "xyes"; then
        echo " - unicode:   enabled"
else
        echo " - unicode:   disabled"
fi

if test "x$have_gtk" = "xyes"; then
        echo " - gtk:       enabled"
else
        echo " - gtk:       disabled"
fi


echo " - mouse support in $mouse_lib"

AC_SUBST(COMMON_PLUGINS_CFLAGS)
AC_SUBST(STATIC_PLUGINS_LIB)
COMMON_PLUGIN_CFLAGS=""
STATIC_PLUGINS_LIB=""

echo 
echo -n "ekg2 plugins will be compilated: "
if test "x$enable_static" = "xyes"; then
	AC_DEFINE_UNQUOTED(STATIC_LIBS, "${PLUGINS}", [define here plugins which will be compilated staticly!])
	echo "as static libs: $PLUGINS"
 
	STATIC_PLUGINS_LIB="$STATIC_PLUGINS_LIB $PLUGINS_LIBS"
 	for i in $PLUGINS; do 
 		STATIC_PLUGINS_LIB="$STATIC_PLUGINS_LIB \$(top_builddir)/plugins/$i/.libs/*.o"
 	done
else
 	AC_DEFINE(SHARED_LIBS, 1, [define if you want shared plugins (in .so or .dll)])
 	COMMON_PLUGINS_CFLAGS="-DPLUGIN_SHARED_LIBS"
dnl 	LIBS="$LIBS -no-undefined"
 
 	echo "as shared libs"
fi
 
AC_OUTPUT
 
