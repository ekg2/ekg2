
AC_DEFUN([AC_PROG_C_STRICT_ALIASING],
[
	AC_SUBST(C_STRICT_ALIASING)

	ac_save_CFLAGS=$CFLAGS
        CFLAGS="-fno-strict-aliasing"

	AC_CACHE_CHECK([whether $CC accepts ${CFLAGS}], C_STRICT_ALIASING,
    	    [_AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
	    [C_STRICT_ALIASING="-fno-strict-aliasing"], [C_STRICT_ALIASING=""])])

	CFLAGS=$ac_save_CFLAGS
])
