AC_DEFUN([AC_EKG2_WITH], [
dnl AC_EKG2_WITH(optname, if-yes, if-no)
dnl Create an ekg2-style '--with-<optname>' option, adding -I & -L flags
dnl as necessary.

	AC_ARG_WITH([$1], [
		AS_HELP_STRING([--with-$1[=<path>]],
			[Build with $1 (locating it in <path>)])
	],, [
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
		m4_popdef([EKG2_FAILED_TEST])
		EKG2_FAILED_TEST
		m4_pushdef([EKG2_FAILED_TEST], [:])
	])

	AS_CASE([$with_$1],
		[yes|maybe], [$2],
		[no], [
			m4_ifval([$3], [$3], [
				m4_ifdef([EKG2_DISABLED_TEST], [
					EKG2_DISABLED_TEST([--without-$1])
				])
			])
		], [
			dnl XXX: multilib?
			CPPFLAGS="$CPPFLAGS -I$with_$1/include"
			LDFLAGS="$LDFLAGS -L$with_$1/lib"

			$2
		]
	)

	m4_popdef([EKG2_FAILED_TEST])
])
