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
			PERL_CFLAGS=`perl -MExtUtils::Embed -e ccopts`
			if test -z "$PERL_CFLAGS"; then
				AC_MSG_RESULT([perl found, but error while getting CFLAGS])
			else
				PERL_LIBS=`perl -MExtUtils::Embed -e ldopts`
				echo "main(){perl_alloc(); return 0;}" > conftest.c
				$CC $CFLAGS conftest.c -o conftest $LDFLAGS $PERL_LIBS 2> perl.error.tmp > /dev/null
				if test ! -s conftest; then
					AC_MSG_RESULT([perl found, but error linking with perl libraries])
				else
					have_perl=yes
				fi
				rm -f perl.error.tmp
			fi
		else
			AC_MSG_RESULT(not found)
		fi
	fi
])
