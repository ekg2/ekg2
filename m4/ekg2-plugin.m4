AC_DEFUN([AC_EKG2_PLUGIN], [
dnl AC_EKG2_PLUGIN(name, req-checks, avail-test, opt-checks)
dnl Create a '--enable-<name>' option for a plugin. Unless disabled, run
dnl <req-checks> and then check their result calling <avail-test>
dnl (as an argument to AS_IF()). If a plugin is enabled and checks
dnl succeed, run <opt-checks> as well.

	AC_ARG_ENABLE([$1], [
		AS_HELP_STRING([--disable-$1], [Disable building of $1 plugin.])
	], [
		AS_CASE([$enableval],
			[yes], [ekg_plugin_$1=yes],
			[no], [ekg_plugin_$1=no],
			[AC_MSG_ERROR([Invalid argument to --enable-$1: $enableval])])
	], [
		ekg_plugin_$1=maybe
	])

	AS_IF([test $ekg_plugin_$1 != no], [
		$2
		AS_IF($3, [ekg_plugin_$1=yes], [
			AS_IF([test $ekg_plugin_$1 = yes], [
				AC_MSG_ERROR([Requirements for plugin $1 not met, aborting build.])
			], [
				AC_MSG_WARN([Requirements for plugin $1 not met, $1 plugin will not be built.])
				ekg_plugin_$1=no
			])
		])
	])

	AS_IF([test $ekg_plugin_$1 != no], [
		$4
	])

	AM_CONDITIONAL([ENABLE_]translit($1, [a-z], [A-Z]), [test $ekg_plugin_$1 != no])
])
