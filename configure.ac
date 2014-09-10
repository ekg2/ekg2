AC_PREREQ([2.60])
AC_INIT([ekg2], [git])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.6 foreign dist-bzip2 nostdinc -Wall])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_PROG_C_NO_STRICT_ALIASING

dnl XXX: docs say we should use '-no-undefined' along with 'win32-dll'
LT_INIT([disable-static win32-dll])

AC_EKG2_WITH([glib], [
	AS_IF([test "x$enable_shared" = "xyes"], [
		gmod_dep="gmodule-2.0"
	], [
		gmod_dep=
	])

	PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.24 gio-2.0 gobject-2.0 $gmod_dep],, [
		AS_CASE([$with_glib],
			[yes|maybe], [with_glib=/usr])

		GLIB_LIBS="-lglib-2.0 -lgobject-2.0 -lgio-2.0"
		AS_IF([test "x$enable_shared" = "xyes"], [
			GLIB_LIBS="$GLIB_LIBS -lgmodule-2.0 -Wl,--export-dynamic"
		])
		GLIB_CFLAGS="-I$with_glib/include/glib-2.0 -I$with_glib/EKG2_LIBDIRNAME/glib-2.0/include"
	])

	AC_EKG2_CHECK_FLAGEXPORTED_LIB([GLIB],, [g_charset_converter_new], [glib.h],, [
		AC_MSG_ERROR([glib2 not found (neither in standard pkgconfig paths nor in $with_glib)])
	])
	AS_UNSET([gmod_dep])
], [
	AC_MSG_ERROR([glib dependency is obligatory, bailing due to --without-glib])
])

AC_CHECK_FUNCS([flock getaddrinfo inet_ntop inet_pton inet_aton])

AC_SEARCH_LIBS([socket], [socket])
AC_CHECK_FUNC([inet_addr],, [AC_CHECK_LIB(bind, __inet_addr, LIBS="$LIBS -lbind")])
AC_CHECK_LIB([wsock32], [main])

AC_CHECK_HEADERS([sys/socket.h ws2tcpip.h])
AC_CHECK_TYPES([socklen_t],,,[
#ifdef HAVE_WS2TCPIP_H
#	include <ws2tcpip.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#	include <sys/socket.h>
#endif
])

AC_CHECK_MEMBERS([struct kinfo_proc.ki_size],,,[
#include <sys/param.h>
#include <sys/user.h>
])

dnl compats -- XXX: get rid of them
AC_CHECK_FUNCS([scandir])

dnl Libraries specific to the ekg2 binary

AC_SEARCH_LIBS([gethostbyname], [nsl])
AC_SEARCH_LIBS([kvm_openfiles], [kvm])
AC_SEARCH_LIBS([sched_yield], [rt])

dnl resolver
AC_CHECK_FUNC([dn_expand], [], [
	AC_CHECK_HEADERS([resolv.h], [
		saved_LIBS="$LIBS"
		RESOLV_LIBS="-lresolv"
		LIBS="$LIBS $RESOLV_LIBS"
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

AC_EKG2_WITH([gnutls], [
	AC_EKG2_CHECK_PKGCONFIG_LIB([gnutls], [gnutls], [gnutls_init], [gnutls/gnutls.h])
])

EKG_LIBS="$LIBS"
AC_SUBST([EKG_LIBS])

dnl We need not to link plugins against the libraries the binary is already linked to.
LIBS=

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.14.3])

dnl gettext still uses it even if other autotools don't
AC_SUBST([MKINSTALLDIRS], ['$(top_srcdir)/build-aux/mkinstalldirs'])

dnl plugins

AC_EKG2_PLUGIN([autoresponder])

AC_EKG2_PLUGIN([gg], [
	AC_EKG2_WITH([libgadu], [
		AC_EKG2_CHECK_PKGCONFIG_LIB([libgadu], [gadu], [gg_logoff], [libgadu.h],, [EKG2_FAILED_TEST])
	])
], [
	AC_CHECK_FUNCS([gg_remind_passwd3 gg_change_passwd4 mkstemp])

	dnl Reason to check for gif or jpeg? They seem to be useless nowadays.
])

AC_EKG2_PLUGIN([gpg], [
	AC_EKG2_WITH([gpgme], [
		AM_PATH_GPGME([1.0.0], [
			AC_EKG2_CHECK_FLAGEXPORTED_LIB([GPGME], [gpgme], [gpgme_new], [gpgme.h],, [EKG2_FAILED_TEST])
		], [EKG2_FAILED_TEST])
	],, [
		AC_MSG_ERROR(['--with-gpgme=<path> is uneffective, please use --with-gpgme-prefix instead'])
	])
])

AC_EKG2_PLUGIN([gtk], [
	AC_EKG2_WITH([gtk], [
		PKG_CHECK_MODULES([GTK], [gtk+-2.0],, [
			AS_CASE([$with_gtk],
				[yes|maybe], [gtk_prefix=/usr],
				[gtk_prefix=$with_gtk])

			GTK_LIBS="-lgtk-x11-2.0"
			GTK_CFLAGS="-I$gtk_prefix/include/gtk-2.0 -I$gtk_prefix/EKG2_LIBDIRNAME/gtk-2.0/include"
			AS_UNSET([gtk_prefix])
		])

		AC_EKG2_CHECK_FLAGEXPORTED_LIB([GTK],, [gtk_init], [gtk/gtk.h], [
			AC_PATH_PROG([GDK_PIXBUF_CSOURCE], [gdk-pixbuf-csource])
			AS_IF([test x"$GDK_PIXBUF_CSOURCE" = x""], [EKG2_FAILED_TEST])
		], [EKG2_FAILED_TEST])
	])
])

AC_EKG2_PLUGIN([icq])

AC_EKG2_PLUGIN([irc])

AC_EKG2_PLUGIN([jabber], [
	AC_EKG2_WITH([expat], [
		AC_EKG2_CHECK_LIB([expat], [XML_ParserCreate], [expat.h],, [EKG2_FAILED_TEST])
	])
], [
	AC_EKG2_WITH([zlib], [
		AC_EKG2_CHECK_PKGCONFIG_LIB([zlib], [z], [gzopen], [zlib.h])
	])
])

AC_EKG2_PLUGIN([jogger])

AC_EKG2_PLUGIN([logs],, [
	AC_EKG2_WITH([zlib], [
		AC_EKG2_CHECK_PKGCONFIG_LIB([zlib], [z], [gzopen], [zlib.h])
	])
])

AC_EKG2_PLUGIN([logsqlite], [
	try_sqlite=no

	AC_EKG2_WITH([sqlite3], [
		AC_EKG2_CHECK_PKGCONFIG_LIB([sqlite3], [sqlite3], [sqlite3_open], [sqlite3.h],, [
			try_sqlite=yes
		])
	], [try_sqlite=yes])

	AS_IF([test $try_sqlite = yes], [
		AC_EKG2_WITH([sqlite], [
			AC_EKG2_CHECK_PKGCONFIG_LIB([sqlite], [sqlite], [sqlite_open], [sqlite.h],, [
				EKG2_FAILED_TEST
			])
		], [EKG2_DISABLED_TEST])
	])

	AS_UNSET([try_sqlite])
])

AC_EKG2_PLUGIN([mail],, [
	AC_EKG2_WITH([inotify], [
		AC_CHECK_FUNCS([inotify_init], [
			AC_CHECK_HEADERS([sys/inotify.h])
		])
	])
])

AC_EKG2_PLUGIN([ncurses], [
	AC_EKG2_WITH([ncurses], [
		AC_SEARCH_LIBS([initscr], [ncursesw ncurses], [
			nc_headers="ncurses/ncurses.h ncurses.h"
			found_any_header=no
			AC_CHECK_FUNCS([waddnwstr], [
				AC_CHECK_HEADERS([ncursesw/ncurses.h ncurses/ncurses.h ncurses.h], [
					found_any_header=yes
					break
				])
			], [
				AC_CHECK_HEADERS([ncurses/ncurses.h ncurses.h], [
					found_any_header=yes
					break
				])
			])

			AS_IF([test $found_any_header = yes], [
				AC_DEFINE([HAVE_NCURSES], [1], [define if you have ncurses])
			], [EKG2_FAILED_TEST])
			AS_UNSET([found_any_header])
			AS_UNSET([nc_headers])
		], [EKG2_FAILED_TEST])
	])
], [
	AC_CHECK_FUNCS([addwstr use_legacy_coding])

	AC_EKG2_WITH([aspell], [
		AC_EKG2_CHECK_LIB([aspell], [new_aspell_config], [aspell.h])
	])

	AC_EKG2_WITH([gpm], [
		AC_EKG2_CHECK_LIB([gpm], [Gpm_GetEvent], [gpm.h])
	])
])

AC_EKG2_PLUGIN([nntp])

perl_module_build_flags=
AC_EKG2_PLUGIN([perl], [
	AC_EKG2_WITH([perl], [
		AC_PATH_PROG([PERL], [perl])
		AS_IF([test x"$PERL", != x""], [
			PERL_CFLAGS=`perl -MExtUtils::Embed -e ccopts`
			PERL_LIBS=`perl -MExtUtils::Embed -e ldopts`

			AC_EKG2_CHECK_FLAGEXPORTED_LIB([PERL], [perl], [perl_alloc], [EXTERN.h],, [EKG2_FAILED_TEST])
		], [EKG2_FAILED_TEST])
	])
], [
	AC_ARG_ENABLE([perl-modules],
		AS_HELP_STRING([--disable-perl-modules],
			[disable building perl modules [default=on]]),
	, [
		enable_perl_modules=yes
	])

	AC_ARG_WITH([perl-module-build-flags],
		AS_HELP_STRING([--with-perl-module-build-flags=...],
			[additional flags passed to Makefile.PL (e.g. INSTALLDIRS)]),
	[
		perl_module_build_flags=$withval
	])
])

AC_SUBST([PERL_MODULE_BUILD_FLAGS], [$perl_module_build_flags])
AM_CONDITIONAL([ENABLE_PERL_MODULES], [test x"$enable_perl_modules" = x"yes"])

AC_EKG2_PLUGIN([polchat])

AC_EKG2_PLUGIN([python], [
	AC_EKG2_WITH([python], [
		AC_PATH_PROGS([PYTHON], [python2 python python2.7 python2.6 python2.5 python2.4])
		AS_IF([test x"$PYTHON" != x""], [
			pyver=`"$PYTHON" -c 'import sys; print "%d.%d" % sys.version_info[[0:2]]'`
			pyfound=no

			dnl python2.7 comes with a pkg-config file, older do not
			PKG_CHECK_MODULES([PYTHON], [python-$pyver], [
				pyfound=yes
			], [
				AC_PATH_PROGS([PYCONF], [python$pyver-config python-config-$pyver])
				AS_IF([test x"$PYCONF" != x""], [
					PYTHON_CFLAGS="`$PYCONF --includes`"
					PYTHON_LIBS="`$PYCONF --ldflags`"
					pyfound=yes
				])
			])

			AS_IF([test $pyfound = yes], [
				AC_EKG2_CHECK_FLAGEXPORTED_LIB([PYTHON],, [Py_Initialize], [Python.h],, [EKG2_FAILED_TEST])
			], [EKG2_FAILED_TEST])

			AS_UNSET([pyfound])
			AS_UNSET([pyver])
		], [EKG2_FAILED_TEST])
	])
])

AC_EKG2_PLUGIN([rc])

AC_EKG2_PLUGIN([readline], [
	AC_EKG2_WITH([readline], [
		AC_EKG2_CHECK_LIB([readline], [readline], [readline/readline.h readline.h],, [EKG2_FAILED_TEST])
	])
], [
	AC_CHECK_TYPES([rl_completion_func_t],,, [[
#include <stdio.h>
#ifdef HAVE_READLINE_READLINE_H
#	include <readline/readline.h>
#else
#	include <readline.h>
#endif
]])
	AC_CHECK_FUNCS([rl_set_prompt rl_filename_completion_function rl_get_screen_size rl_set_key rl_bind_key_in_map])
])

AC_EKG2_PLUGIN([rivchat])

AC_EKG2_PLUGIN([rss], [
	AC_EKG2_WITH([expat], [
		AC_EKG2_CHECK_LIB([expat], [XML_ParserCreate], [expat.h],, [EKG2_FAILED_TEST])
	])
])

AC_EKG2_PLUGIN([sim], [
	AC_EKG2_WITH([openssl], [
		AC_EKG2_CHECK_PKGCONFIG_LIB([openssl], [ssl], [RSA_new], [openssl/ssl.h],,, [
			LIBS="-lssl -lcrypto $LIBS"
			AC_DEFINE([HAVE_LIBSSL], [1], [define if you want to use openssl for ssl connections])
		])
	])
])

AC_EKG2_PLUGIN([sms])

dnl CPPFLAGS were duplicated into whatever_CPPFLAGS
dnl make the global ones exe-specific
EKG_CPPFLAGS="$CPPFLAGS"
AC_SUBST([EKG_CPPFLAGS])
CPPFLAGS=

AC_CONFIG_COMMANDS_PRE([
	AM_CONDITIONAL([ENABLE_GCH], [test "x$enable_dependency_tracking" = "xno"])
	AS_IF([test "x$shrext_cmds" != "x"], [
		AC_DEFINE_UNQUOTED([PLUGIN_SUFFIX], ["$shrext_cmds"],
				[set to plugin suffix if known])
	])
])
AC_CONFIG_HEADERS([ekg2-config.h])
AC_CONFIG_FILES([Makefile po/Makefile.in])
AC_OUTPUT
