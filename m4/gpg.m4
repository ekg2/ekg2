AC_DEFUN([AC_CHECK_GPG],
[
	AC_PATH_PROG(GPG, gpg)

	if test "$GPG" != no; then
		AC_DEFINE_UNQUOTED(GPG_PATH, "$GPG", [Define PATH to GnuPG binary.])
		have_gpg=yes
	fi 

	if test "x$have_gpg" != "xyes"; then 
		AC_MSG_RESULT(not found)
 	fi
])

