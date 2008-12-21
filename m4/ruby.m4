dnl RUBY_LIBS=`$RUBY -r mkmf -e 'c=Config::CONFIG; libs=c[["libdir"]]+"/"+c[["LIBRUBY"]]+" "+c[["LIBS"]]; print libs'`
dnl RUBY_LIBS XXX

RUBY=
RUBY_CFLAGS=
RUBY_LIBS=

AC_DEFUN([AM_CHECK_RUBY],
[
	AC_SUBST(RUBY_LIBS)
	AC_SUBST(RUBY_CFLAGS)

	if test "x$with_ruby" != "xno"; then
		AC_PATH_PROG(RUBY, ruby)
		if test "$RUBY" != ""; then
			ruby_cflags_h=`$RUBY -r mkmf -e 'print Config::CONFIG[["archdir"]]'`
			if test -z "$ruby_cflags_h"; then
				AC_MSG_RESULT([ruby found, but error while getting CFLAGS])
			else
				RUBY_CFLAGS="-I$ruby_cflags_h"
				ruby_libs_r=`$RUBY -r mkmf -e 'c=Config::CONFIG; libs=c[["RUBY_SO_NAME"]]+" "+c[["LIBS"]]; print libs'`
				RUBY_LIBS="-l$ruby_libs_r"

				echo "main(){ruby_init(); return 0;}" > conftest.c
				$CC $CFLAGS conftest.c -o conftest $LDFLAGS $RUBY_LIBS 2> ruby.error.tmp > /dev/null
				if test ! -s conftest; then
					AC_MSG_RESULT([ruby found, but error linking with ruby libraries])
				else
					have_ruby=yes
				fi
				rm -f ruby.error.tmp
			fi
		else
			AC_MSG_RESULT(not found)
		fi
	fi
])
