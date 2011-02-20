ekg2-checks.m4
========================================================================

`AC_EKG2_CHECK_LIB`
------------------------------------------------------------------------

	AC_EKG2_CHECK_LIB(name, func, headers, [if-yes], [if-no])

A wrapper for `AC_CHECK_LIB` with additional `AC_CHECK_HEADERS` test.
First checks if _any_ of header files listed in `headers` is usable
and if it is, checks for function `func` in library `name`.

If both tests succeed, `if-yes` is evaluated. If it is empty,
the default success action of `AC_CHECK_LIB` is performed (`-l<name>` is
appended to `LIBS` and `HAVE_LIB<name>` is declared).

If any of the tests fail, `if-no` is evaluated. It defaults to no-op.

Example:

	dnl 1) check for <ncurses/ncurses.h> or <ncurses.h>
	dnl 2) check for initscr in -lncurses
	dnl on success: -lncurses to libs, declare HAVE_LIBNCURSES
	dnl on failure: expand EKG_FAILED_TEST (see below)

	AC_EKG2_CHECK_LIB([ncurses], [initscr],
			[ncurses/ncurses.h ncurses.h],,
			[EKG2_FAILED_TEST])


`AC_EKG2_CHECK_FLAGEXPORTED_LIB`
------------------------------------------------------------------------

	AC_EKG2_CHECK_FLAGEXPORTED_LIB(variable-prefix, lib-name, func,
			header, [if-yes], [if-no])

Perform tests similar to `AC_EKG2_CHECK_LIB` for a package whose CFLAGS
& LIBS were exported as `<variable-prefix>_CFLAGS`
& `<variable_prefix>_LIBS` (e.g. by pkg-config).

This macro uses `AC_CHECK_HEADER` to check for `header`, and then
`AC_CHECK_FUNCS` to check for `func` function. The correct `-l...` flags
are supposed to be in the `*_LIBS` variable already, `lib-name` is used
for the macro name only.

If the tests succeed, `LIBS` and `CPPFLAGS` are updated with values
from the prefixed variables. If `lib-name` is provided, macro
`HAVE_LIB<lib-name>` is declared. Afterwards, `if-yes` is evaluated.

If either of the tests fail, `LIBS` and `CPPFLAGS` are unchanged
and `if-no` is evaluated.

This macro is mostly used internally by `AC_EKG2_CHECK_PKGCONFIG_LIB`
but it might be useful if a package does provide custom `*-config`
script. For example:

	dnl AM_PATH_GPGME declares GPGME_CFLAGS & GPGME_LIBS
	dnl we apply them and check for gpgme.h and gpgme_new()
	dnl on success: declare HAVE_LIBGPGME
	dnl on failure: restore CPPFLAGS & LIBS, then EKG2_FAILED_TEST

	AM_PATH_GPGME([1.0.0], [
		AC_EKG2_CHECK_FLAGEXPORTED_LIB([GPGME], [gpgme], [gpgme_new],
				[gpgme.h],, [EKG2_FAILED_TEST])
	], [EKG2_FAILED_TEST])


`AC_EKG2_CHECK_PKGCONFIG_LIB`
------------------------------------------------------------------------

	AC_EKG2_CHECK_PKGCONFIG_LIB(pkg-name, fallback-name, func,
			header, [if-yes], [if-no], [if-fallback-yes])

Wrapper around `PKG_CHECK_MODULES` with fallback to `AC_EKG2_CHECK_LIB`.

Checks for pkg-config module `pkg-name`. If it is found, grabs CFLAGS
& LIBS grabbed by pkg-config, checks for `headers` header and then
for `func` function. If it is not, tries to find the header with
standard CPPFLAGS and the function within `fallback-name` library.

If either of the methods succeed (i.e. both header & function is found),
`HAVE_LIB<fallback-name>` is declared and `CPPFLAGS` & `LIBS`
are updated.

If pkg-config was unable to find the package but fallback succeeded,
and `if-fallback-yes` is nonempty, it is evaluated. If it is empty,
or pkg-config succeeds, `if-yes` is evaluated.

If none of the methods is able to find both the header and the function,
`if-no` is evaluated instead.

Typical example:

	dnl Try to find libgadu.pc, fallback to -lgadu.
	dnl In any case, look for <libgadu.h> and gg_logoff().
	dnl On success: declare HAVE_LIBGADU, set CPPFLAGS & LIBS.
	dnl On failure: EKG2_FAILED_TEST.

	AC_EKG2_CHECK_PKGCONFIG_LIB([libgadu], [gadu], [gg_logoff],
			[libgadu.h],, [EKG2_FAILED_TEST])

A more complex example:

	dnl Try openssl.pc, fallback to -lssl,
	dnl but in the latter case, remember to add -lcrypto as well.

	AC_EKG2_CHECK_PKGCONFIG_LIB([openssl], [ssl], [RSA_new],
			[openssl/ssl.h],,, [
		LIBS="-lssl -lcrypto $LIBS"
		AC_DEFINE([HAVE_LIBSSL], [1], [blah blah blah])
	])


ekg2-with.m4
========================================================================

`AC_EKG2_MULTILIB`
------------------------------------------------------------------------

	AC_EKG2_MULTILIB

Internal use macro which sets `EKG2_LIBDIRNAME` macro up.

Techically, it checks whether `$libdir` ends with either `lib64`
or `lib32`, and makes `EKG2_LIBDIRNAME` evaluate to either one of these
two, or to simple `lib` otherwise.


`EKG2_LIBDIRNAME`
------------------------------------------------------------------------

	EKG2_LIBDIRNAME

Evaluates to (possibly) correct libdir for a particular system.

Example use:

	GLIB_CFLAGS="... -I/usr/EKG2_LIBDIRNAME/glib-2.0/include"


`EKG2_FAILED_TEST`
------------------------------------------------------------------------

	EKG2_FAILED_TEST

Macro declared by `AC_EKG2_WITH` within `if-yes` scope. It is supposed
to be used whenever the tests required for the package fail, to disable
the relevant option. It _does not_ terminate the test scope.


`AC_EKG2_WITH`
------------------------------------------------------------------------

	AC_EKG2_WITH(optname, if-yes, [if-no],
			[alt-setup], [alt-cleanup])

Adds an EKG2-style `--with-<optname>` option, supporting both
enabling/disabling a particular dependency and specifying an alternate
prefix for it.

The `if-yes` argument specifies the tests which are supposed to be
performed for a particular package controlled by the option.
If the tests fail, the `EKG2_FAILED_TEST` macro should be used.

If neither `--with-*` nor `--without-*` option is specified, the tests
specified in `if-yes` are evaluated. If they fail, `with_<optname>` is
set to `no`. If macro `EKG2_FAILED_PLUGIN` is declared, it is expanded
as well then.

If `--without-*` or `--with-*=no` is specified, `if-no` is evaluated
instead. If it is empty and `EKG2_DISABLED_TEST` macro is declared, it
is expanded.

If simple `--with-*` or `--with-*=yes` is specified, the tests
in `if-yes` are performed. However, if `EKG2_FAILED_TEST` is called,
the configure process is terminated immediately due to unsatisfied
dependency.

Otherwise, if `--with-*=<prefix>` is specified, tests are performed
as above. If `alt-setup` is empty, `CPPFLAGS`, `LDFLAGS`, `PATH`
and pkg-config paths are adjusted to prefix beforewards, and restored
afterwards. If it is nonempty, `alt-setup` is expanded beforewards,
and `alt-cleanup` afterwards instead.

Example:

	dnl --with-expat

	AC_EKG2_WITH([expat], [
		AC_EKG2_CHECK_LIB([expat], [XML_ParserCreate],
				[expat.h],, [EKG2_FAILED_TEST])
	])

ekg2-plugin.m4
========================================================================

`AC_EKG2_PLUGIN_SETUP`
------------------------------------------------------------------------

	AC_EKG2_PLUGIN_SETUP

Internal use macro which sets up plugin support. It is invoked
automatically before first use of `AC_EKG2_PLUGIN`.


`EKG2_FAILED_PLUGIN`
------------------------------------------------------------------------

	EKG2_FAILED_PLUGIN

Macro declared by `AC_EKG2_PLUGIN` in `req-checks` scope. It is supposed
to be used whenever tests for obligatory plugin requirements fail. Much
like `EKG2_FAILED_TEST`, it does not terminate the test scope. It is
called automatically by `EKG2_FAILED_TEST` if available.


`EKG2_DISABLED_TEST`
------------------------------------------------------------------------

	EKG2_DISABLED_TEST

Macro declared by `AC_EKG2_PLUGIN` in `req-checks` scope. It is supposed
to be used whenever obligatory plugin requirements are disabled through
`--without-*` options. It does not terminate the test scope. By default,
it is called automatically by `AC_EKG2_WITH`. It is used to detect
conflicting options.


`AC_EKG2_PLUGIN`
------------------------------------------------------------------------

	AC_EKG2_PLUGIN(name, req-checks, opt-checks)

Declare and initiate plugin `name`. Perform obligatory checks specified
in `req-checks`, and then optional dependency checks in `opt-checks`.

This macro creates `--enable-<name>` option for the plugin. If it is
explicitly enabled and the plugin tests fail, configure process is
aborted. If no option is specified and the checks fail, the plugin is
disabled. If `--disable-*` option is specified, the plugin is disabled
without performing the checks.

The `req-checks` are supposed to check for _all_ obligatory plugin
dependencies and should call `EKG2_FAILED_PLUGIN` (or in most cases,
`EKG2_FAILED_TEST` within `AC_EKG2_WITH` as a wrapper for it) if any
of the required dependencies is unsatisfied.

The `opt-checks` are supposed to check for any optional plugin
dependencies. These are performed only if obligatory checks succeed,
and they _should not_ use `EKG2_FAILED_TEST`.

The `AC_EKG2_PLUGIN` macro scopes `CPPFLAGS`, `LDFLAGS` and `LIBS`.
If the tests succeed, it exports them as automake variables
for the appropriate .la rule. It also declares `<name>dir` (e.g.
`jabberdir`) for plugin data (e.g. help files), and sets up static
plugin support if necessary.

In any case, `ENABLE_<name>` automake conditional is declared.
It evaluates to either true or false depending on whether the particular
plugin is enabled (and obligatory dependency tests succeeded).

<!-- vim:set syn=markdown : -->
