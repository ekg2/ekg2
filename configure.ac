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

AC_SYS_LARGEFILE

dnl It's obsolete in current versions of gettext but it's needed in older
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.14.3])

ALL_LINGUAS="pl"

dnl little or big endian ? 
AC_C_BIGENDIAN

AC_SUBST(CONTRIB_SUBDIRS)
AC_SUBST(REMOTE_SUBDIR)
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

dnl linux, sunos
AC_CHECK_FUNC([dn_expand], [], [
	AC_CHECK_HEADERS([resolv.h], [
		saved_LIBS="$LIBS"
		LIBS="$LIBS -lresolv"
		AC_MSG_CHECKING([[if libresolv is usable]])
		dnl we cannot just use AC_CHECK_LIB, because dn_expand is
		dnl implemented as a macro on certain platforms, and will not
		dnl be resolved by linker without including necessary headers
		AC_LINK_IFELSE([
			AC_LANG_PROGRAM([[#include <resolv.h>]],
					[[dn_expand(0,0,0,0,0);]])
			],[
				AC_MSG_RESULT([yes])
				AC_DEFINE(HAVE_LIBRESOLV, 1, [define if you have libresolv])
			],[
				AC_MSG_RESULT([no])
				AC_MSG_WARN([[resolver library unusable, SRV record resolution - e.g. for jabber plugin - will not work]])
				LIBS="$saved_LIBS"
			]
		)
	],[
		AC_MSG_WARN([[resolver library not found, SRV record resolution - e.g. for jabber plugin - will not work]])
	])
])

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
dnl AC_CHECK_FUNCS(strlen, , [COMPAT="$COMPAT strlen.o"])
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

dnl core 
AC_CONFIG_FILES([Makefile ekg/Makefile remote/Makefile contrib/Makefile compat/Makefile docs/Makefile plugins/Makefile po/Makefile.in])
dnl protocol-plugins
AC_CONFIG_FILES([plugins/feed/Makefile plugins/gg/Makefile plugins/icq/Makefile plugins/irc/Makefile plugins/jabber/Makefile plugins/polchat/Makefile plugins/rivchat/Makefile])
dnl ui-plugins
AC_CONFIG_FILES([plugins/gtk/Makefile plugins/httprc_xajax/Makefile plugins/ncurses/Makefile plugins/rc/Makefile plugins/readline/Makefile plugins/remote/Makefile])
dnl log-plugins
AC_CONFIG_FILES([plugins/logs/Makefile plugins/logsoracle/Makefile plugins/logsqlite/Makefile])
dnl scripting-plugins and samples-scripts
AC_CONFIG_FILES([plugins/perl/Makefile plugins/python/Makefile])
AC_CONFIG_FILES([contrib/python/Makefile contrib/perl/Makefile])
dnl crypt-plugins
AC_CONFIG_FILES([plugins/gpg/Makefile plugins/rot13/Makefile plugins/sim/Makefile])
dnl misc-plugins
AC_CONFIG_FILES([plugins/mail/Makefile plugins/sms/Makefile plugins/pcm/Makefile plugins/xmsg/Makefile plugins/ioctld/Makefile plugins/gsm/Makefile plugins/xosd/Makefile plugins/autoresponder/Makefile plugins/jogger/Makefile])

PLUGINS="logs mail rc sms pcm icq irc polchat rot13 httprc_xajax xmsg autoresponder jogger rivchat remote"
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

AC_ARG_ENABLE([remote],		AC_HELP_STRING([--enable-remote],		[Build ekg2-remote (EXPERIMENTAL)]))

if test "x$enable_remote" = "xyes"; then
   REMOTE_SUBDIR="remote"
   AC_SUBST(REMOTE_SSL_LIBS)
   AC_SUBST(REMOTE_SSL_CPPFLAGS)

   if test "x$have_gnutls" = "xyes" -a "x$with_libgnutls" != "xno"; then
       remote_have_gnutls=yes;
       REMOTE_SSL_LIBS=$LIBGNUTLS_LIBS
       REMOTE_SSL_CPPFLAGS=$LIBGNUTLS_CFLAGS

       AC_DEFINE([REMOTE_WANT_GNUTLS], 1, [define if you want use gnutls for ssl connections])
   else
      if test "x$have_openssl" = "xyes"; then
	  remote_have_openssl=yes;
	  REMOTE_SSL_LIBS=$OPENSSL_LIBS
	  REMOTE_SSL_CPPFLAGS=$OPENSSL_INCLUDES
	  AC_DEFINE([REMOTE_WANT_OPENSSL], 1, [define if you want use openssl for ssl connections])
      fi
   fi
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

AC_ARG_ENABLE([skip_relative_plugins_dir],
    AC_HELP_STRING([--enable-skip-relative-plugins-dir], [Do not load plugins from directories relative to CWD]),
    [
        if test "x$enableval" = "xno"; then
            : do not define the macro
        else
            AC_MSG_NOTICE([plugins will not be loaded from relative directories])
            AC_DEFINE([SKIP_RELATIVE_PLUGINS_DIR], 1, [Do not load plugins from directories relative to CWD])
        fi
    ]
)
 
AC_OUTPUT

#
# set variables for cEcho
#
if test "x$EKG2_NO_COLOR_CONFIGURE" = "x" && which tput >/dev/null 2>&1; then
	clr_r=$(tput sgr0;tput setaf 1)
	clr_g=$(tput sgr0;tput setaf 2)
	clr_y=$(tput sgr0;tput setaf 3)
	clr_b=$(tput sgr0;tput setaf 4)
	clr_m=$(tput sgr0;tput setaf 5)
	clr_c=$(tput sgr0;tput setaf 6)
	clr_k=$(tput sgr0;tput setaf 7)
	clr_R=$(tput bold;tput setaf 1)
	clr_G=$(tput bold;tput setaf 2)
	clr_Y=$(tput bold;tput setaf 3)
	clr_B=$(tput bold;tput setaf 4)
	clr_M=$(tput bold;tput setaf 5)
	clr_C=$(tput bold;tput setaf 6)
	clr_K=$(tput bold;tput setaf 7)
	clr_0=$(tput sgr0)
fi

cEcho() {
	for i in "$@"; do
		text=eval printf "%b" "$i"|sed "
		s/%r/$clr_r/g;
		s/%g/$clr_g/g;
		s/%y/$clr_y/g;
		s/%b/$clr_b/g;
		s/%m/$clr_m/g;
		s/%c/$clr_c/g;
		s/%k/$clr_k/g;
		s/%R/$clr_R/g;
		s/%G/$clr_G/g;
		s/%Y/$clr_Y/g;
		s/%B/$clr_B/g;
		s/%M/$clr_M/g;
		s/%C/$clr_C/g;
		s/%K/$clr_K/g;
		s/%0/$clr_0/g";

		printf "%s" "$text"
	done
	printf "%s" "${clr_0}"; # set default colour
}

echo_title() {
	cEcho "%Y$@"
}

echo_section() {
	cEcho "%g$@"
}

echo_with() {
	cEcho "%c$@"
}

echo_have() {
	cEcho "%G$@"
}

echo_err() {
	cEcho "%R$@"
}

echo_disabled() {
	cEcho "%b$@"
}

#
#
#
echo_title "\nConfigured options:\n"

echo_section " CFLAGS: "
echo_have "$CFLAGS\n"

echo_section " EXTRA CFLAGS FOR PLUGINS: "
echo_have "$C_FVISIBILITY\n"

echo_section " LDFLAGS: "
echo_have "$LDFLAGS\n"

echo_section " LIBS: "
echo_have "$DLLIB $LIBS\n"

echo_title "PLUGINS:\n"
# - UI PLUGINS
echo_section " - Interface:\t"
d_plg=""
	if test "x$have_ncurses" = "xyes"; then
		echo_have "ncurses "

		if test "x$have_aspell" = "xyes"; then
		        echo_with "(with aspell) "
		fi
	else
		d_plg="ncurses "
	fi

	if test "x$have_gtk" = "xyes"; then
	        echo_have "gtk "
	else
		d_plg="${d_plg}gtk "
	fi

	if test "x$have_readline" = "xyes"; then
		echo_have "readline "
	else
		d_plg="${d_plg}readline "
	fi

	if test "x$d_plg" != "x"; then
		echo_disabled "(disabled: $d_plg)"
	fi
echo ""

# - PROTOCOL PLUGINS
echo_section " - Protocols:\t"
echo_have "icq irc polchat xmsg "
d_plg=""
	if test "x$have_expat" = "xyes"; then
		echo_have "feed jabber "

		if test "x$jabber_have_gnutls" = "xyes"; then
			echo_with "(with ssl support in -lgnutls) "
		fi

		if test "x$jabber_have_openssl" = "xyes"; then
			echo_with "(with ssl support in -lssl) "
		fi
	else
		d_plg="${d_plg}feed jabber "
	fi

	if test "x$have_libgadu" = "xyes"; then
		echo_have "gg "

		if test "x$have_pthread" = "xyes"; then
			echo_with "(with pthread resolver) "
		fi
	else
		d_plg="${d_plg}gg "
	fi 

	if test "x$d_plg" != "x"; then
		echo_disabled "(disabled: $d_plg)"
	fi
echo ""

# - LOGGING PLUGINS
echo_section " - Logging:\t"
d_plg=""
	echo_have "logs "
	if test "x$have_zlib" = "xyes"; then
		echo_with "(with support for zlib compression) "
	fi

	if test "x$have_sqlite" = "xyes"; then
		echo_have "logsqlite "
	elif test "x$have_sqlite3" = "xyes"; then
		echo_have "logsqlite "
		echo_with "(using sqlite3) "
	else
		d_plg="${d_plg}logsqlite "
	fi

	if test "x$have_logsoracle" = "xyes"; then
		echo_have "logsoracle "
	else
		d_plg="${d_plg}logsoracle "
	fi

	if test "x$d_plg" != "x"; then
		echo_disabled "(disabled: $d_plg)"
	fi
echo ""

# - AUDIO PLUGINS
echo_section " - Audio:\t"
echo_have "pcm "
d_plg=""
	if test "x$have_libgsm" = "xyes"; then
		echo_have "libgsm "
	else
		d_plg="${d_plg}libgsm "
	fi

	if test "x$d_plg" != "x"; then
		echo_disabled "(disabled: $d_plg)"
	fi
echo ""

# - ENCRYPTION PLUGINS
echo_section " - Encryption:\t"
echo_have "rot13 "
d_plg=""
	if test "x$have_openssl" = "xyes"; then
		echo_have "sim "
	else
		d_plg="${d_plg}sim "
	fi

	if test "x$have_gpg" = "xyes"; then
		echo_have "gpg "
	else
		d_plg="${d_plg}gpg "
	fi

	if test "x$d_plg" != "x"; then
		echo_disabled "(disabled: $d_plg)"
	fi
echo ""

# - SCRIPTING PLUGINS
echo_section " - Scripting:\t"
d_plg=""
	if test "x$have_python" = "xyes"; then
		echo_have "python "
	else
		d_plg="${d_plg}python "
	fi

	if test "x$have_perl" = "xyes"; then
		echo_have "perl "
	else
		d_plg="${d_plg}perl "
	fi

dnl	if test "x$have_ruby" = "xyes"; then
dnl		echo_have "ruby "
dnl	fi
	if test "x$d_plg" != "x"; then
		echo_disabled "(disabled: $d_plg)"
	fi
echo ""

# - MISC PLUGINS
echo_section " - Misc:\t"
echo_have "autoresponder jogger mail rc sms "
d_plg=""
	if test "x$have_ioctld_includes" = "xyes"; then
	 	echo_have "ioctld "
	else
		d_plg="${d_plg}iotld "
	fi

	if test "x$have_libxosd" = "xyes"; then
		echo_have "xosd "
	else
		d_plg="${d_plg}xosd "
	fi

	if test "x$d_plg" != "x"; then
		echo_disabled "(disabled: $d_plg)"
	fi
echo ""

#########################
echo ""

if test "x$enable_remote" = "xyes"; then
	echo_section " - REMOTE:\t"
	echo_have "tcp,unix"

	if test "x$remote_have_gnutls" = "xyes"; then
		echo_have ",tcps "
		echo_with "(with ssl support in -lgnutls) "
	fi

	if test "x$remote_have_openssl" = "xyes"; then
		echo_have ",tcps "
		echo_with "(with ssl support in -lssl) "
	fi
	echo ""
fi

echo_section " - NLS system:\t"
if test "$USE_NLS" = "yes"; then
	echo_have "en pl"
else 
	echo_err "disabled, only en"
fi
echo ""


if test "x$have_libgadu" = "xyes"; then
	echo_section " - GG Tokens:\t"
	echo_have "saving to file"

	if test "x$have_libjpeg" = "xyes"; then
	        echo_have ", JPEG OCR "
	        echo_with "(using: -ljpeg)"
	fi

	if test "x$have_gif" = "xyes"; then
		echo_have ", GIF OCR "
		echo_with "(using: $GG_TOKENGIF_LIBS)"
	fi
	echo ""
fi

if test "x$have_ncurses" = "xyes"; then
	echo_section " - Mouse support:"
	echo_have "in $mouse_lib\n"
fi

echo_section " - Charset support:\t"
	if test "x$enable_unicode" = "xyes"; then
	        echo_have "utf-8&ansi (EXPERIMENTAL)"
	else
	        echo_have "iso-8859-2"
	fi
echo ""

echo_section " - Plugins:\t "
	if test "x$enable_static" = "xyes"; then
		echo_have "as static libs"
	else
	 	echo_have "as shared libs"
	fi
echo "" 

echo
if test "x$am_cv_func_iconv" != "xyes"; then
	echo_err "******************\n"
	echo_err "* BIG FAT WARNING\n"
	echo_err "* EKG2 will be compiled without iconv support. This means most charset\n"
	echo_err "* conversion magic won't work. Jabber would work only on UTF-8 systems, and GG\n"
	echo_err "* only on UTF-8, ISO-8859-2 or CP1250 ones. If you want something more, get\n"
	echo_err "* yourself some iconv. Else, have fun.\n"
	echo_err "******************\n"
fi
