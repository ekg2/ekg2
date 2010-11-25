dnl $Id$

AC_DEFUN([AC_PROG_C_STD],
[
	AC_SUBST(C_STD)

	ac_save_CFLAGS=$CFLAGS
	C_STD=""

	for std_val in gnu99 c99
	do
	        CFLAGS="-std=$std_val"

		AC_MSG_CHECKING([whether $CC accepts -std=$std_val])
		_AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],[answer=yes],[answer=no])
		AC_MSG_RESULT([$answer])
		if test "$answer" = "yes" ; then
			C_STD="$std_val"
			break
		fi
	done

	CFLAGS=$ac_save_CFLAGS
])

