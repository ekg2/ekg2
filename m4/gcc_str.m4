dnl $Id$

AC_DEFUN([AC_PROG_C_STR],
[
	AC_SUBST(C_STR)

	ac_save_CFLAGS=$CFLAGS
        CFLAGS="-str=c99"
	
	AC_CACHE_CHECK(whether $CC accepts -str, C_STR,
    	    [_AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
	    [C_STR=yes], [C_STR=no])])

	CFFLAGS=$ac_save
])

