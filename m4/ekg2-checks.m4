AC_DEFUN([AC_EKG2_CHECK_LIB], [
dnl AC_EKG2_CHECK_LIB(name, func, headers, [if-yes], [if-no])
dnl Perform AC_CHECK_HEADERS & AC_CHECK_LIB for a particular library. Run
dnl 'if-yes' if both checks succeed (if not specified, defaults to AC_CHECK_LIB
dnl default commands); otherwise run 'if-no'.

	found_any_header=no

	AC_CHECK_HEADERS([$3], [
		AC_CHECK_LIB([$1], [$2], [$4], [$5])

		found_any_header=yes
		break
	])

	AS_IF([test $found_any_header != yes], [
		$5
	])
	AS_UNSET([found_any_header])
])
