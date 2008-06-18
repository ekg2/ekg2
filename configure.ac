dnl $Id$

AC_INIT(ekg/ekg.c)
AC_PREREQ(2.50)
AC_CONFIG_HEADERS(ekg2-config.h)

AM_INIT_AUTOMAKE(ekg2, CVS)

dnl we don't use neither fortran neither cxx ... set it to no for disable checking for it..
F77="no"
CXX="no"

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_C_CONST

AC_LIBTOOL_DLOPEN
AC_DISABLE_STATIC

AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

dnl It's obsolete in current versions of gettext but it's needed in older
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.14.3])

ALL_LINGUAS="pl"

dnl little or big endian ? 
AC_C_BIGENDIAN

AC_SUBST(CONTRIB_SUBDIRS)
AC_SUBST(COMPAT)
AC_SUBST(DLLIB)
AC_SUBST(PLUGINS)

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

AC_CHECK_FUNCS(flock)

dnl locale / encoding... etc
AM_LANGINFO_CODESET

CFLAGS="$CFLAGS -Wall"

AC_PROG_C_STD

if test "x$C_STD" = "xyes"; then
	CFLAGS="$CFLAGS -std=c99"
fi

AC_PROG_C_FVISIBILITY

dnl  mkstemp do zapisywania tokenów przy braku libjpeg
AC_CHECK_FUNCS(mkstemp)

dnl regex.h do regexpowania ;-)
dnl XXX, sprawdzic czy nie musimy sprawdzac funkcji
AC_CHECK_HEADERS(regex.h)

AC_CHECK_MEMBERS([struct kinfo_proc.ki_size],,,[
#include <sys/param.h>
#include <sys/user.h>
])

dnl Maybe we should rather check both includes alone, and then define which one we should use
dnl and blah blah blah, but I think we'll always include both files
AC_CHECK_TYPE(socklen_t,[AC_DEFINE(HAVE_SOCKLEN_T, 1, [define if you've got socklen_t])],,[
#include <sys/types.h>
#include <sys/socket.h>
])

AC_CHECK_FUNC(dlopen, [], [AC_CHECK_LIB(dl, dlopen, DLLIB="$DLLIB -ldl")])

AM_ICONV

AC_CHECK_LIBIDN

AC_CONFIG_FILES([Makefile ekg/Makefile compat/Makefile docs/Makefile po/Makefile.in])
AC_CONFIG_FILES([contrib/Makefile contrib/python/Makefile contrib/perl/Makefile])
AC_CONFIG_FILES([plugins/Makefile plugins/polchat/Makefile plugins/mail/Makefile plugins/logs/Makefile plugins/rc/Makefile plugins/sms/Makefile plugins/pcm/Makefile plugins/rot13/Makefile plugins/irc/Makefile plugins/httprc_xajax/Makefile plugins/xmsg/Makefile plugins/ioctld/Makefile plugins/gg/Makefile plugins/sim/Makefile plugins/jabber/Makefile plugins/ncurses/Makefile plugins/gtk/Makefile plugins/readline/Makefile plugins/gsm/Makefile plugins/xosd/Makefile plugins/perl/Makefile plugins/python/Makefile plugins/logsoracle/Makefile plugins/logsqlite/Makefile plugins/gpg/Makefile plugins/autoresponder/Makefile plugins/feed/Makefile plugins/jogger/Makefile])

PLUGINS="logs mail rc sms pcm irc polchat rot13 httprc_xajax xmsg autoresponder jogger"
export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig"

AC_CHECK_PROG([PKGCONFIG], [pkg-config], [pkg-config], [no])

dnl ##############################################################################################################################

mouse_lib="xterm only"
AC_ARG_WITH(gpm-mouse,		AC_HELP_STRING([--without-gpm-mouse],		[Compile without gpm mouse support (Linux only)]))

case $host_os in
linux*)
    if test x$with_gpm_mouse != no; then
        AC_CHECK_LIB(gpm, Gpm_GetEvent,
            [AC_DEFINE(HAVE_LIBGPM, 1,
                       [Define to enable gpm mouse support on Linux])
            mouse_lib="gpm and xterm"
            LIBGPM_LIBS="-lgpm"
	    AC_SUBST(LIBGPM_LIBS)],
            [AC_MSG_WARN([libgpm is missing])

        ])
    fi
    ;;
esac

AC_ARG_WITH(default-theme,	AC_HELP_STRING([--with-default-theme=theme],	[Use default theme]))

if test "x$with_default_theme" != "x"; then
	AC_DEFINE_UNQUOTED(DEFAULT_THEME, "$with_default_theme", [define your default theme])
fi

AC_ARG_WITH(theme,		AC_HELP_STRING([--without-theme],		[Don't compile default theme]))

if test "x$with_theme" = "xno"; then
	AC_DEFINE(NO_DEFAULT_THEME, 1, [define if you don't want default theme])
	AC_MSG_WARN([You don't build default theme!! remember about ekg2 -t or see --with-theme option in ./configure])
fi
dnl
dnl  Resolver libgadu oparty na pthread
dnl

AC_ARG_WITH(pthread,		AC_HELP_STRING([--without-pthread],		[Don't use pthread in resolver]))

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

AC_ARG_WITH(aspell,		AC_HELP_STRING([--without-aspell],		[Disable aspell support]))

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

AC_ARG_WITH(gif,		AC_HELP_STRING([--without-gif],			[Compile without GIF token support]))
AC_ARG_WITH(libgif,		AC_HELP_STRING([--without-libgif],		[Compile without libgif token support]))
AC_ARG_WITH(libungif,		AC_HELP_STRING([--without-libungif],		[Compile without libungif token support]))

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

AC_ARG_WITH(libjpeg, 		AC_HELP_STRING([--without-libjpeg],		[Compile without JPEG token support]))

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

AC_ARG_WITH(libgnutls,		AC_HELP_STRING([--without-libgnutls],		[Compile without libgnutls support (TLS in jabber)]))
if test "x$with_libgnutls" != "xno"; then
	AM_PATH_LIBGNUTLS( 1.0.0,,AC_MSG_WARN([[
***
*** libgnutls was not found. You may want to get it from
*** ftp://ftp.gnutls.org/pub/gnutls/
	]]))
fi

AC_ARG_WITH(zlib,		AC_HELP_STRING([--without-zlib],		[Compile without zlib support (in logs plugin)]))

if test "x$with_zlib" != "xno"; then
	AC_CHECK_LIB(z, gzopen, [
		AC_CHECK_HEADERS(zlib.h, [
			AC_DEFINE(HAVE_ZLIB, 1, [define if you want compressed logs])
			AC_SUBST(ZLIB_LIBS)
			ZLIB_LIBS="-lz"
			have_zlib=yes
		])
	])
fi

AC_ARG_WITH(inotify,		AC_HELP_STRING([--without-inotify],		[Compile without inotify support (in xmsg plugin)]))

if test "x$with_inotify" != "xno"; then
	AC_CHECK_HEADERS(sys/inotify.h, [
		AC_DEFINE(HAVE_INOTIFY, 1, [define if you want inotify support])
	])
fi

AC_ARG_ENABLE([unicode],	AC_HELP_STRING([--enable-unicode],		[Enable unicode (EXPERIMENTAL)]),
    [
        if test "x$enable_unicode" = "xyes"; then
            AC_DEFINE([USE_UNICODE], 1, [Use unicode (wide character strings)])
        fi
    ]
)

dnl #######################################################################################################################

AC_ARG_WITH(ioctld,		AC_HELP_STRING([--without-ioctld],		[Compile without ioctld (ioctld plugin)]))

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
		PLUGINS="$PLUGINS ioctld"
	fi
fi

AC_ARG_WITH(libgadu,		AC_HELP_STRING([--without-libgadu],		[Compile without libgadu (GG plugin)]))

if test "x$with_libgadu" != "xno"; then
	AC_CHECK_LIBGADU
	
	if test "x$have_libgadu" = "xyes"; then
		PLUGINS="$PLUGINS gg"
	fi
fi

AC_ARG_WITH(openssl,		AC_HELP_STRING([--without-openssl],		[Compile without OpenSSL support (sim plugin)]))

if test "x$with_openssl" != "xno"; then
	AC_CHECK_OPENSSL

	if test "x$have_openssl" = "xyes"; then
		PLUGINS="$PLUGINS sim"
	fi
fi

AC_ARG_WITH(expat,		AC_HELP_STRING([--without-expat],		[Compile without expat (jabber&feed plugins)]))

if test "x$with_expat" != "xno"; then
	AC_CHECK_EXPAT

	if test "x$have_expat" = "xyes"; then
		AC_SUBST(JABBER_SSL_LIBS)
		AC_SUBST(JABBER_SSL_CPPFLAGS)

		if test "x$have_gnutls" = "xyes" -a "x$with_libgnutls" != "xno"; then
			jabber_have_gnutls=yes;
			JABBER_SSL_LIBS=$LIBGNUTLS_LIBS
			JABBER_SSL_CPPFLAGS=$LIBGNUTLS_CFLAGS

			AC_DEFINE([JABBER_HAVE_GNUTLS], 1, [define if you want use gnutls for ssl connections])
		else
			if test "x$have_openssl" = "xyes"; then
				jabber_have_openssl=yes;
				JABBER_SSL_LIBS=$OPENSSL_LIBS
				JABBER_SSL_CPPFLAGS=$OPENSSL_INCLUDES
				AC_DEFINE([JABBER_HAVE_OPENSSL], 1, [define if you want use openssl for ssl connections])
			fi
		fi
		PLUGINS="$PLUGINS jabber feed"
	fi
fi

AC_ARG_WITH(ncurses,		AC_HELP_STRING([--without-ncurses],		[Compile without ncurses support (ncurses plugin)]))

if test "x$with_ncurses" != "xno"; then
	AC_CHECK_NCURSES

	if test "x$have_ncurses" = "xyes"; then
		PLUGINS="$PLUGINS ncurses"
		AC_CHECK_HEADERS(curses.h ncurses.h ncurses/ncurses.h)
	fi
fi

AC_ARG_WITH(gtk,		AC_HELP_STRING([--without-gtk],			[Compile without gtk2 support (gtk plugin)]))

if test "x$with_gtk" != "xno"; then
	AC_CHECK_GTK2([], [have_gtk="yes"], [have_gtk=x"no"], []) 
	if test "x$have_gtk" = "xyes"; then
		PLUGINS="$PLUGINS gtk"
		AC_DEFINE(HAVE_GTK, 1, [define if you have gtk2])
	fi
fi

AC_ARG_WITH(readline,		AC_HELP_STRING([--without-readline],		[Compile without readline support (readline plugin)]))

if test "x$with_readline" != "no"; then
	AC_CHECK_READLINE

	if test "x$have_readline" = "xyes"; then
		PLUGINS="$PLUGINS readline"
dnl backup CFLAGS && LDFLAGS
		ac_save_CFLAGS="$CFLAGS"
		ac_save_LDFLAGS="$LDFLAGS"
dnl new CFLAGS && LDFLAGS
		CFLAGS="$CFLAGS $READLINE_INCLUDES"
	        LDFLAGS="$READLINE_LIBS $LDFLAGS"
dnl check funcs
		AC_CHECK_FUNCS([rl_set_prompt rl_filename_completion_function rl_get_screen_size rl_set_key rl_bind_key_in_map])
dnl restore CFLAGS && LDFLAGS
		CFLAGS="$ac_save_CFLAGS"
		LDFLAGS="$ac_save_LDFLAGS"
	fi
fi

AC_ARG_WITH(libgsm,		AC_HELP_STRING([--without-libgsm],		[Compile without libgsm support (gsm plugin)]))

if test "x$with_libgsm" != "xno"; then
	AC_CHECK_LIBGSM

	if test "x$have_libgsm" = "xyes"; then
		PLUGINS="$PLUGINS gsm"
	fi
fi


AC_ARG_WITH(libxosd,		AC_HELP_STRING([--without-libxosd],		[Compile without libxosd support (xosd plugin)]))

if test "x$with_libxosd" != "xno"; then
	AM_CHECK_LIBXOSD

	if test "x$have_libxosd" = "xyes"; then
		PLUGINS="$PLUGINS xosd"
	fi
fi

AC_ARG_WITH(perl,		AC_HELP_STRING([--without-perl],		[Compile without perl support (perl plugin)]))

if test "x$with_perl" != "xno"; then
	AM_CHECK_PERL

	if test "x$have_perl" = "xyes"; then
		CONTRIB_SUBDIRS="$CONTRIB_SUBDIRS perl"
		PLUGINS="$PLUGINS perl"
	fi
fi

AC_ARG_WITH(python,		AC_HELP_STRING([--without-python],		[Compile without python support (python plugin)]))

if test "x$with_python" != "xno"; then
	AM_CHECK_PYTHON

	if test "x$have_python" = "xyes"; then
		CONTRIB_SUBDIRS="$CONTRIB_SUBDIRS python"
		PLUGINS="$PLUGINS python"
	fi
fi

dnl AC_ARG_WITH(ruby,		AC_HELP_STRING([--without-ruby],		[Compile without ruby support (ruby plugin)]))
dnl 
dnl if test "x$with_ruby" != "xno"; then
dnl	AM_CHECK_RUBY
dnl
dnl 	if test "x$have_ruby" = "xyes"; then
dnl		PLUGINS="$PLUGINS ruby"
dnl 	fi
dnl fi

AC_ARG_WITH(sqlite3,		AC_HELP_STRING([--without-sqlite3],		[Compile without sqlite3 support (sqlite plugin)]))

if test "x$with_sqlite3" != "xno"; then
	AC_CHECK_SQLITE3
	if test "x$have_sqlite3" = "xyes"; then
		AC_DEFINE(HAVE_SQLITE3, 1, [define if you have libsqlite3])
	fi
fi

AC_ARG_WITH(sqlite,		AC_HELP_STRING([--without-sqlite],		[Compile without sqlite support (sqlite plugin)]))

if test "x$have_sqlite3" != "xyes"; then
	if test "x$with_sqlite" != "xno"; then
		AC_CHECK_SQLITE
	fi	
fi	
if test "x$have_sqlite" = "xyes" || test "x$have_sqlite3" = "xyes" ; then
	PLUGINS="$PLUGINS logsqlite"
fi

AC_ARG_WITH(gpg,		AC_HELP_STRING([--without-gpg],			[Compile without gpg support (gpg plugin)]))

if test "x$with_gpg" != "xno"; then
	AM_PATH_GPGME(1.0.0, [
		have_gpg="yes"
		PLUGINS="$PLUGINS gpg"
	])
fi

dnl  'logsoracle' plugin
dnl
dnl  Libs required:	pthread, clntsh
dnl  Other:		$ORACLE_HOME must be set
dnl  Default state:	turned off
dnl	
AC_ARG_WITH([logsoracle],     [AC_HELP_STRING([--with-logsoracle], [log messages/status changes into an Oracle database @<:@default=no@:>@])],
	    [with_logsoracle=yes],
	    [with_logsoracle=no])

if test "x$with_logsoracle" == "xyes"; then
	AC_CHECK_ORACLE
else
	AC_MSG_RESULT([skipping logsoracle plugin])
fi

PLUGINS_LIBS=""

for i in $PLUGINS; do
	PLUGINS_LIBS="$PLUGINS_LIBS \$(top_builddir)/plugins/$i/$i.la"
done

plugindir=$libdir/ekg2/plugins
AC_SUBST(plugindir)

# STATIC / DYNAMIC plugins 

AC_SUBST(COMMON_PLUGINS_CFLAGS)
AC_SUBST(STATIC_PLUGINS_LIB)
COMMON_PLUGIN_CFLAGS=""
STATIC_PLUGINS_LIB=""

if test "x$enable_static" = "xyes"; then
	AC_DEFINE_UNQUOTED(STATIC_LIBS, "${PLUGINS}", [define here plugins which will be compilated staticly!])
 
	STATIC_PLUGINS_LIB="$STATIC_PLUGINS_LIB $PLUGINS_LIBS"
 	for i in $PLUGINS; do 
 		STATIC_PLUGINS_LIB="$STATIC_PLUGINS_LIB \$(top_builddir)/plugins/$i/.libs/*.o"
 	done
else
 	AC_DEFINE(SHARED_LIBS, 1, [define if you want shared plugins (in .so or .dll)])
 	COMMON_PLUGINS_CFLAGS="-DPLUGIN_SHARED_LIBS"
dnl 	LIBS="$LIBS -no-undefined"
fi
 
AC_OUTPUT

echo
echo "configured options: "

echo "CFLAGS: $CFLAGS"
echo "EXTRA CFLAGS FOR PLUGINS: $C_FVISIBILITY"
echo "LDFLAGS: $LDFLAGS"
echo "LIBS: $DLLIB $LIBS"
echo "PLUGINS: "
# - UI PLUGINS
echo -en " - Interface:\t "
	if test "x$have_ncurses" = "xyes"; then
		echo -n "ncurses "

		if test "x$have_aspell" = "xyes"; then
		        echo -n "(with aspell) "
		fi
	fi

	if test "x$have_gtk" = "xyes"; then
	        echo -n "gtk "
	fi

	if test "x$have_readline" = "xyes"; then
		echo -n "readline "
	fi
echo ""

# - PROTOCOL PLUGINS
echo -en " - Protocols:\t irc polchat xmsg "
	if test "x$have_expat" = "xyes"; then
		echo -n "feed jabber "

		if test "x$jabber_have_gnutls" = "xyes"; then
			echo -n "(with ssl support in -lgnutls) "
		fi

		if test "x$jabber_have_openssl" = "xyes"; then
			echo -n "(with ssl support in -lssl) "
		fi
	fi

	if test "x$have_libgadu" = "xyes"; then
		echo -n "gg "

		if test "x$have_pthread" = "xyes"; then
			echo -n "(with pthread resolver) "
		fi
	fi 
echo ""

# - LOGGING PLUGINS
echo -en " - Logging:\t "
	echo -n "logs "
	if test "x$have_zlib" = "xyes"; then
		echo -n "(with support for zlib compression) "
	fi

	if test "x$have_sqlite" = "xyes"; then
		echo -n "logsqlite "
	elif test "x$have_sqlite3" = "xyes"; then
		echo -n "logsqlite (using sqlite3) "
	fi

	if test "x$have_logsoracle" = "xyes"; then
		echo -n "logsoracle "
	fi
echo ""

# - AUDIO PLUGINS
echo -en " - Audio:\t pcm "
	if test "x$have_libgsm" = "xyes"; then
		echo -n "libgsm "
	fi
echo ""

# - ENCRYPTION PLUGINS
echo -en " - Encryption:\t rot13 "
	if test "x$have_openssl" = "xyes"; then
		echo -n "sim "
	fi

	if test "x$have_gpg" = "xyes"; then
		echo -n "gpg "
	fi
echo ""

# - SCRIPTING PLUGINS
echo -en " - Scripting:\t "
	if test "x$have_python" = "xyes"; then
		echo -n "python "
	fi

	if test "x$have_perl" = "xyes"; then
		echo -n "perl "
	fi

dnl	if test "x$have_ruby" = "xyes"; then
dnl		echo -n "ruby "
dnl	fi
echo ""

# - MISC PLUGINS
echo -en " - Misc:\t autoresponder jogger mail rc sms "
	if test "x$have_ioctld_includes" = "xyes"; then
	 	echo -n "ioctld "
	fi

	if test "x$have_libxosd" = "xyes"; then
		echo -n "xosd "
	fi
echo ""

#########################
echo ""

echo -en " - NLS system:\t "
if test "$USE_NLS" = "yes"; then
	echo "en pl"
else 
	echo "disabled, only en"
fi

if test "x$have_libgadu" = "xyes"; then
	echo -en " - GG Tokens:\t saving to file"

	if test "x$have_libjpeg" = "xyes"; then
	        echo -n ", JPEG OCR (using: -ljpeg)"
	fi

	if test "x$have_gif" = "xyes"; then
		echo -n ", GIF OCR (using: $GG_TOKENGIF_LIBS)"
	fi
	echo ""
fi

if test "x$have_ncurses" = "xyes"; then
	echo -e " - Mouse support:in $mouse_lib"
fi

echo -en " - Charset support:\t "
	if test "x$enable_unicode" = "xyes"; then
	        echo "utf-8&ansi (EXPERIMENTAL)"
		echo "*****************************************************************************************************"
		echo "WARNING: USING THE RESULTING EKG2 BINARY IN A NON-UTF-8 (ASCII or LATIN) TERMINAL IS CURRENTLY"
		echo "NOT SUPPORTED WHEN BUILT WITH GCC 4.x OR LATER."
		echo "This is due to a known bug in the ncurses UI module - a symptom is ekg2 going into AN INFINITE LOOP."
		echo "Workarounds:"
		echo " - specify --disable-unicode if you do not need UTF-8 support"
		echo " - pass -U to ekg2 to force UTF-8 output to terminal."
		echo " - use a UTF-8 terminal (e.g. uxterm)"
		echo " - disable strict aliasing rules (-fno-strict-aliasing in CFLAGS)"
		echo " - disable optimization (-O1 in CFLAGS is enough)"
		echo " - build with older GCC (3.4 seems to be OK)"
		echo "IT'S A KNOWN BUG, PLEASE DON'T REPORT IT. FEEL FREE TO SEND IN A PATCH."
		echo "IT'LL EVENTUALLY BE FIXED. SOMEDAY."
		echo "******************************************************************************************************"
		echo "AND ENJOY USING EKG2-UNICODE"
	else
	        echo "iso-8859-2"
	fi

echo -en " - Plugins:\t "
	if test "x$enable_static" = "xyes"; then
		echo "as static libs"
	else
	 	echo "as shared libs"
	fi
 
echo 
if test "x$am_cv_func_iconv" != "xyes"; then
	echo "******************"
	echo "* BIG FAT WARNING"
	echo "* EKG2 will be compiled without iconv support. This means most charset"
	echo "* conversion magic won't work. Jabber would work only on UTF-8 systems, and GG"
	echo "* only on UTF-8, ISO-8859-2 or CP1250 ones. If you want something more, get"
	echo "* yourself some iconv. Else, have fun."
	echo "******************"
fi
