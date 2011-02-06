dnl Stupid debian pkg-config-0.22
m4_pattern_allow([PKG_CONFIG_LIBDIR])
m4_pattern_allow([PKG_CONFIG_PATH])

AC_DEFUN([AC_EKG2_MULTILIB], [
dnl AC_EKG2_MULTILIB()
dnl Initialize multilib support, i.e. find out what libdir to use.

	# XXX: actually support multilib
	ac_ekg2_multilib_libdir=lib
])

AC_DEFUN([EKG2_LIBDIRNAME], [AC_REQUIRE([AC_EKG2_MULTILIB])$ac_ekg2_multilib_libdir])

AC_DEFUN([AC_EKG2_WITH], [
dnl AC_EKG2_WITH(optname, if-yes, [if-no], [alt-setup], [alt-cleanup])
dnl Create an ekg2-style '--with-<optname>' option. Run <if-no> if
dnl '--without-<optname>' was used, <if-yes> otherwise.
dnl
dnl If '--with-<optname>=' was specified with a path, either run <alt-setup>
dnl before running <if-yes> if specified or set up -I, -L flags and pkg-config
dnl vars. In the same case, <alt-cleanup> is being run after <if-yes> if
dnl <alt-setup> was specified, or pkg-config vars are cleaned up.

	AC_ARG_WITH([$1],
		AS_HELP_STRING([translit([--with-$1[=<prefix>]], [_], [-])],
			[build with $1 (in <prefix>) [default=auto]]),
	, [
		with_$1=maybe
	])

	m4_pushdef([EKG2_FAILED_TEST], [
		AS_CASE([$with_$1],
			[no|maybe], [
				with_$1=no
			], [
				AC_MSG_ERROR([Test for --with-$1 failed, aborting build.])
			]
		)

		m4_ifdef([EKG2_FAILED_PLUGIN], [
			EKG2_FAILED_PLUGIN
		])
	])

	AS_CASE([$with_$1],
		[yes|maybe], [$2],
		[no], [
			m4_default([$3], [
				m4_ifdef([EKG2_DISABLED_TEST], [
					EKG2_DISABLED_TEST([--without-$1])
				])
			])
		], [
			m4_default([$4], [
				CPPFLAGS="$CPPFLAGS -I$with_$1/include"
				LDFLAGS="$LDFLAGS -L$with_$1/EKG2_LIBDIRNAME"

				ekg_saved_PKG_CONFIG_LIBDIR=${PKG_CONFIG_LIBDIR+yes}
				ekg_save_PKG_CONFIG_PATH=$PKG_CONFIG_PATH
				ekg_save_PKG_CONFIG_LIBDIR=$PKG_CONFIG_LIBDIR

				AS_UNSET([PKG_CONFIG_PATH])
				PKG_CONFIG_LIBDIR="$with_$1/EKG2_LIBDIRNAME/pkgconfig:$with_$1/share/pkgconfig"
				export PKG_CONFIG_LIBDIR
			])

			$2

			m4_ifval([$4], [$5], [
				# pkg-config differentiates between unset & empty PKG_CONFIG_LIBDIR
				AS_IF([test "$ekg_saved_PKG_CONFIG_LIBDIR" = "yes"], [
					PKG_CONFIG_LIBDIR=$ekg_save_PKG_CONFIG_LIBDIR
					export PKG_CONFIG_LIBDIR
				], [
					AS_UNSET([PKG_CONFIG_LIBDIR])
				])
				PKG_CONFIG_PATH=$ekg_save_PKG_CONFIG_PATH
				export PKG_CONFIG_PATH
			])
		]
	)

	m4_popdef([EKG2_FAILED_TEST])
])
