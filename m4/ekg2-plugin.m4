AC_DEFUN([AC_EKG2_PLUGIN_SETUP], [
	AC_SUBST([EKG2_STATIC_PLUGIN_LIBS])

	AS_IF([test "x$enable_static" = "xyes"], [
		AC_DEFINE([STATIC_LIBS], [1], [define if you want static plugins])
		EKG2_STATIC_PLUGIN_DECLS=
		EKG2_STATIC_PLUGIN_CALLS=
		AC_CONFIG_COMMANDS_PRE([
			AC_DEFINE_UNQUOTED([STATIC_PLUGIN_DECLS], [$EKG2_STATIC_PLUGIN_DECLS], [static plugin init function declarations])
			AC_DEFINE_UNQUOTED([STATIC_PLUGIN_CALLS], [$EKG2_STATIC_PLUGIN_CALLS], [static plugin init function calls])
		])
	])

	AS_IF([test "x$enable_shared" = "xyes"], [
		AC_DEFINE([SHARED_LIBS], [1], [define if you want shared plugins (in .so or .dll)])
	])
])

AC_DEFUN([AC_EKG2_PLUGIN], [
dnl AC_EKG2_PLUGIN(name, req-checks, opt-checks)
dnl Create a '--enable-<name>' option for a plugin. Unless disabled, run
dnl <req-checks>. If a plugin is enabled and checks succeed, run <opt-checks>
dnl as well.
dnl
dnl The <req-checks> part is supposed to call EKG2_FAILED_TEST if the checks
dnl fail. It can also call EKG2_DISABLED_TEST if the necessary checks
dnl (dependencies) were explicitly disabled by user.
dnl
dnl CPPFLAGS & LIBS will be saved and restored on termination. If tests
dnl succeed, they will be copied as well to $1_CPPFLAGS and $1_LIBS,
dnl and AC_SUBSTituted with that names.
	
	AC_REQUIRE([AC_EKG2_PLUGIN_SETUP])

	AC_ARG_ENABLE([$1],
		AS_HELP_STRING([--disable-$1], [disable building of $1 plugin [default=auto]]),
	, [
		enable_$1=maybe
	])

	AS_IF([test $enable_$1 != no], [
		ac_ekg2_plugin_save_CPPFLAGS=$CPPFLAGS
		ac_ekg2_plugin_save_LDFLAGS=$LDFLAGS
		ac_ekg2_plugin_save_LIBS=$LIBS

		m4_pushdef([EKG2_FAILED_PLUGIN], [
			AS_IF([test $enable_$1 = yes], [
				AC_MSG_ERROR([Requirements for plugin $1 not met, aborting build.])
			], [
				AC_MSG_WARN([Requirements for plugin $1 not met, $1 plugin will not be built.])
				enable_$1=no
			])
		])

		m4_pushdef([EKG2_DISABLED_TEST], [
			dnl XXX: get somehow $1 inner expansion
			AS_IF([test $enable_$1 = yes], [
				AC_MSG_ERROR([--without-* conflicts with --enable-$1])
			], [
				AC_MSG_WARN([Requirements for plugin $1 disabled, $1 plugin will not be built.])
				enable_$1=no
			])
		])

		$2

		m4_popdef([EKG2_FAILED_PLUGIN])
		m4_popdef([EKG2_DISABLED_TEST])

		AS_IF([test $enable_$1 != no], [
			$3

			AC_SUBST([plugins_$1_$1_la_CPPFLAGS], [$CPPFLAGS])
			AC_SUBST([plugins_$1_$1_la_LDFLAGS], ["-module -avoid-version $LDFLAGS"])
			AC_SUBST([plugins_$1_$1_la_LIBADD], [$LIBS])
			AC_SUBST([$1dir], ['$(pkgdatadir)/plugins/$1'])

			AS_IF([test "x$enable_static" = "xyes"], [
				EKG2_STATIC_PLUGIN_DECLS="$EKG2_STATIC_PLUGIN_DECLS G_MODULE_IMPORT int $1_plugin_init(int);"
				EKG2_STATIC_PLUGIN_CALLS="$EKG2_STATIC_PLUGIN_CALLS if (!xstrcmp(name, \"$1\")) plugin_init = &$1_plugin_init;"
				EKG2_STATIC_PLUGIN_LIBS="$EKG2_STATIC_PLUGIN_LIBS plugins/$1/.libs/$1.a"
				EKG_LIBS="$EKG_LIBS $LIBS"
			])
		])

		CPPFLAGS=$ac_ekg2_plugin_save_CPPFLAGS
		LDFLAGS=$ac_ekg2_plugin_save_LDFLAGS
		LIBS=$ac_ekg2_plugin_save_LIBS
	])

	AM_CONDITIONAL([ENABLE_]translit($1, [a-z], [A-Z]), [test $enable_$1 != no])
])
