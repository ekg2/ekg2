AC_DEFUN([EKG2_FAILED_TEST], [
	m4_fatal_error([EKG2_FAILED_TEST used outside of AC_EKG2_PLUGIN])
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

	AC_ARG_ENABLE([$1], [
		AS_HELP_STRING([--disable-$1], [Disable building of $1 plugin.])
	],, [
		enable_$1=maybe
	])

	AS_IF([test $enable_$1 != no], [
		ac_ekg2_plugin_save_CPPFLAGS=$CPPFLAGS
		ac_ekg2_plugin_save_LIBS=$LIBS

		m4_pushdef([EKG2_FAILED_TEST], [
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

		m4_popdef([EKG2_FAILED_TEST])

		AS_IF([test $enable_$1 != no], [
			$3

			$1_CPPFLAGS=$CPPFLAGS
			$1_LIBS=$LIBS

			AC_SUBST(translit($1, [a-z], [A-Z])[_CPPFLAGS], [$$1_CPPFLAGS])
			AC_SUBST(translit($1, [a-z], [A-Z])[_LIBS], [$$1_LIBS])
		])

		CPPFLAGS=$ac_ekg2_plugin_save_CPPFLAGS
		LIBS=$ac_ekg2_plugin_save_LIBS
	])

	AM_CONDITIONAL([ENABLE_]translit($1, [a-z], [A-Z]), [test $enable_$1 != no])
])
