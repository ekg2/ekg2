dnl $Id$

AC_DEFUN(AC_CHECK_NCURSES,
[
	AC_SUBST(NCURSES_LIBS)
	AC_SUBST(NCURSES_CFLAGS)

	AC_ARG_WITH(ncurses, [[  --with-ncurses[=dir]  Compile with ncurses/locate base dir]], [
		if test "x$withval" = "xno"; then
			without_ncurses=yes
		elif test "x$withval" != "xyes"; then
			NCURSES_CFLAGS="-I$withval/include"
			NCURSES_LIBS="-I$withval/lib"
		fi
	])

	if test "x$without_ncurses" != "xyes"; then
		cflags="$CFLAGS $NCURSES_CFLAGS"
		ldflags="$LDFLAGS $NCURSES_LIBS"
		have_ncurses_h=""
		AC_CHECK_HEADERS([ncurses.h],
		[
			have_ncurses_h="yes"
		], [
			AC_CHECK_HEADERS([ncurses/ncurses.h],
			[
				have_ncurses_h="yes"
			])
		])

		if test "x$have_ncurses_h" = "xyes"; then
			AC_CHECK_LIB([ncurses], [initscr],
			[
				AC_DEFINE([HAVE_NCURSES], 1, [define if you have ncurses])
				NCURSES_LIBS="$NCURSES_LIBS -lncurses"
				have_ncurses="yes"
			])
		fi

		$as_unset have_ncurses_h

		CFLAGS="$cflags"
		LDFLAGS="$ldflags"
	fi
])

