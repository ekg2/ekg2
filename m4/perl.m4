dnl 

PERL=
PERL_CFLAGS=
PERL_LIBS=

AC_DEFUN([AM_CHECK_PERL],
[
	AC_SUBST(PERL_LIBS)
	AC_SUBST(PERL_CFLAGS)

	if test "x$with_perl" != "xno"; then
		AC_PATH_PROG(PERL, perl)
		if test "$PERL" != ""; then
			PERL_LIBS=`perl -MExtUtils::Embed -e ldopts`
			PERL_CFLAGS=`perl -MExtUtils::Embed -e ccopts`
			
			AC_DEFINE(WITH_PERL, 1, [define if You want perl])
			have_perl=yes
		fi

		if test "x$have_perl" != "xyes"; then 
			AC_MSG_RESULT(not found)
		fi
	fi
])
