
AC_DEFUN([AC_PROG_C_FVISIBILITY],
[
	AC_SUBST(C_FVISIBILITY)

	ac_save_CFLAGS=$CFLAGS
        CFLAGS="-fvisibility=hidden"
	
	AC_CACHE_CHECK(whether $CC accepts -fvisibility, C_FVISIBILITY,
    	    [_AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
	    [C_FVISIBILITY="-fvisibility=hidden"], [C_FVISIBILITY=""])])

	CFLAGS=$ac_save_CFLAGS
])
