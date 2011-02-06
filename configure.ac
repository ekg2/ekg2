AC_PREREQ([2.60])
AC_INIT([ekg2], [git])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.6 foreign dist-bzip2])

AC_PROG_CC_C99
dnl XXX: docs say we should use '-no-undefined' along with 'win32-dll'
LT_INIT([disable-static win32-dll])

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.16 gmodule-2.0])
LIBS="$LIBS $GLIB_LIBS"
CFLAGS="$CFLAGS $GLIB_CFLAGS"

AC_CHECK_HEADERS(regex.h)
AC_CHECK_FUNCS([flock getaddrinfo inet_ntop inet_pton inet_aton])

AC_SEARCH_LIBS([socket], [socket])
AC_CHECK_FUNC([inet_addr],, [AC_CHECK_LIB(bind, __inet_addr, LIBS="$LIBS -lbind")])
AC_CHECK_LIB([wsock32], [main])

AC_CHECK_TYPE([socklen_t],,,[
#include <sys/types.h>
#include <sys/socket.h>
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

AC_EKG2_WITH([libidn], [
	dnl XXX: pkg-config

	AC_CHECK_HEADER([idna.h], [
		AC_CHECK_LIB([idn], [stringprep_check_version])
	])
])

EKG_LIBS="$LIBS"
AC_SUBST([EKG_LIBS])

dnl We need not to link plugins against the libraries the binary is already linked to.
LIBS=

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.14.3])
AM_ICONV
AM_LANGINFO_CODESET

dnl gettext still uses it even if other autotools don't
AC_SUBST([MKINSTALLDIRS], ['$(top_srcdir)/build-aux/mkinstalldirs'])

AC_ARG_ENABLE([skip_relative_plugins_dir],
	AS_HELP_STRING([--enable-skip-relative-plugins-dir],
		[do not load plugins from directories relative to CWD]),
[
	AS_IF([test "x$enableval" != "xno"], [
		AC_MSG_NOTICE([plugins will not be loaded from relative directories])
		AC_DEFINE([SKIP_RELATIVE_PLUGINS_DIR], [1], [do not load plugins from directories relative to CWD [default=off]])
	])
])

AC_ARG_ENABLE([unicode],
	AS_HELP_STRING([--disable-unicode],
		[disable building with unicode support [default=on]]),
, [
	enable_unicode=yes
])
AS_IF([test "x$enable_unicode" = "xyes"], [
	AC_DEFINE([USE_UNICODE], [1], [use unicode (wide character strings)])
])

AS_IF([test "x$enable_shared" = "xyes"], [
	AC_DEFINE([SHARED_LIBS], [1], [define if you want shared plugins (in .so or .dll)])
])
AS_IF([test "x$enable_static" = "xyes"], [
	AC_MSG_ERROR([Static plugins not supported currently])
])

dnl plugins

AC_EKG2_PLUGIN([autoresponder])

AC_EKG2_PLUGIN([feed],, [
	AC_EKG2_WITH([expat], [
		AC_EKG2_CHECK_LIB([expat], [XML_ParserCreate], [expat.h],, [EKG2_FAILED_TEST])
	])
])

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

AC_EKG2_PLUGIN([icq])

AC_EKG2_PLUGIN([irc],, [
	AC_EKG2_WITH([openssl], [
		AC_EKG2_CHECK_PKGCONFIG_LIB([openssl], [ssl], [RSA_new], [openssl/ssl.h],,, [
			LIBS="-lssl -lcrypto $LIBS"
			AC_DEFINE([HAVE_LIBSSL], [1], [define if you want to use openssl for ssl connections])
		])
	])
])

AC_EKG2_PLUGIN([jabber], [
	AC_EKG2_WITH([expat], [
		AC_EKG2_CHECK_LIB([expat], [XML_ParserCreate], [expat.h],, [EKG2_FAILED_TEST])
	])
], [
	AC_EKG2_WITH([zlib], [
		AC_EKG2_CHECK_LIB([z], [gzopen], [zlib.h])
	])

	try_openssl=no

	AC_EKG2_WITH([gnutls], [
		AC_EKG2_CHECK_PKGCONFIG_LIB([gnutls], [gnutls], [gnutls_init], [gnutls/gnutls.h],, [
			try_openssl=yes
		])
	], [try_openssl=yes])

	AS_IF([test $try_openssl = yes], [
		AC_EKG2_WITH([openssl], [
			AC_EKG2_CHECK_PKGCONFIG_LIB([openssl], [ssl], [RSA_new], [openssl/ssl.h],,, [
				LIBS="-lssl -lcrypto $LIBS"
				AC_DEFINE([HAVE_LIBSSL], [1], [define if you want to use openssl for ssl connections])
			])
		])
	])

	AS_UNSET([try_openssl])
])

AC_EKG2_PLUGIN([jogger])

AC_EKG2_PLUGIN([logs],, [
	AC_EKG2_WITH([zlib], [
		AC_EKG2_CHECK_LIB([z], [gzopen], [zlib.h])
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
		dnl AC_EKG2_NCURSES([nlib], [nheaders])
		AC_DEFUN([AC_EKG2_NCURSES], [
			AC_EKG2_CHECK_LIB([$1], [initscr], [$2], [
				LIBS="-l$1 $LIBS"
				AC_DEFINE([HAVE_NCURSES], [1], [define if you have ncurses])
			], [EKG2_FAILED_TEST])
		])

		AS_IF([test "$enable_unicode" = "yes"], [
			AC_EKG2_NCURSES([ncursesw], [ncursesw/ncurses.h])
		], [
			AC_EKG2_NCURSES([ncurses], [ncurses/ncurses.h ncurses.h])
		])
	])
], [
	AC_CHECK_FUNCS([use_legacy_coding])

	AC_EKG2_WITH([aspell], [
		AC_EKG2_CHECK_LIB([aspell], [new_aspell_config], [aspell.h])
	])

	AC_EKG2_WITH([gpm], [
		AC_EKG2_CHECK_LIB([gpm], [Gpm_GetEvent], [gpm.h])
	])
])

AC_EKG2_PLUGIN([perl], [
	AC_EKG2_WITH([perl], [
		AC_PATH_PROG([PERL], [perl])
		AS_IF([test x"$PERL", != x""], [
			PERL_CFLAGS=`perl -MExtUtils::Embed -e ccopts`
			PERL_LIBS=`perl -MExtUtils::Embed -e ldopts`

			AC_EKG2_CHECK_FLAGEXPORTED_LIB([PERL], [perl], [perl_alloc], [EXTERN.h],, [EKG2_FAILED_TEST])
		], [EKG2_FAILED_TEST])
	])
])

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
				AC_PATH_PROG([PYCONF], [python-config-$pyver])
				AS_IF([test x"$PYCONF" != x""], [
					PYTHON_CFLAGS="`$PYCONF --includes`"
					PYTHON_LIBS="`$PYCONF --libs`"
					pyfound=yes
				])
			])

			AS_IF([test $pyfound = yes], [
				dnl $2 is just for define name
				AC_EKG2_CHECK_FLAGEXPORTED_LIB([PYTHON], [python], [Py_Initialize], [Python.h],, [EKG2_FAILED_TEST])
			], [EKG2_FAILED_TEST])

			AS_UNSET([pyfound])
			AS_UNSET([pyver])
		], [EKG2_FAILED_TEST])
	])
])

AC_EKG2_PLUGIN([readline], [
	AC_EKG2_WITH([readline], [
		AC_EKG2_CHECK_LIB([readline], [readline], [readline/readline.h readline.h],, [EKG2_FAILED_TEST])
	])
], [
	AC_CHECK_FUNCS([rl_set_prompt rl_filename_completion_function rl_get_screen_size rl_set_key rl_bind_key_in_map])
])

AC_EKG2_PLUGIN([rivchat])

AC_EKG2_PLUGIN([sim], [
	AC_EKG2_WITH([openssl], [
		AC_EKG2_CHECK_PKGCONFIG_LIB([openssl], [ssl], [RSA_new], [openssl/ssl.h],,, [
			LIBS="-lssl -lcrypto $LIBS"
			AC_DEFINE([HAVE_LIBSSL], [1], [define if you want to use openssl for ssl connections])
		])
	])
])

AC_EKG2_PLUGIN([sms])

AC_CONFIG_HEADERS([ekg2-config.h])
AC_CONFIG_FILES([Makefile po/Makefile.in])
AC_OUTPUT
