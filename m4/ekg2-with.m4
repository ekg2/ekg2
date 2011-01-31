AC_DEFUN([AC_EKG2_WITH], [
dnl AC_EKG2_WITH(optname, if-yes, if-no)
dnl Create an ekg2-style '--with-<optname>' option, adding -I & -L flags
dnl as necessary.

	AC_ARG_WITH([$1], [
		AS_HELP_STRING([--with-$1[=<path>]],
			[Build with $1 (locating it in <path>)])
	], [
		with_$1=$withval
	], [
		with_$1=yes
	])

	AS_CASE([$with_$1],
		[yes], [$2],
		[no], [$3],
		[
			dnl XXX: multilib?
			CPPFLAGS="$CPPFLAGS -I$with_$1/include"
			LIBS="$LIBS -L$with_$1/lib"

			$2
		]
	)
])
