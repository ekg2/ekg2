dnl $Id$

AC_DEFUN([AC_CHECK_NCURSES],
[
	AC_SUBST(NCURSES_LIBS)
	AC_SUBST(NCURSES_CPPFLAGS)

	AC_ARG_WITH(ncurses, [[  --with-ncurses[=dir]  Compile with ncurses/locate base dir]], [
		if test "x$withval" = "xno"; then
			without_ncurses=yes
		elif test "x$withval" != "xyes"; then
			NCURSES_CPPFLAGS="-I$withval/include"
			NCURSES_LIBS="-L$withval/lib"
		fi
	])

	if test "x$without_ncurses" != "xyes"; then
		save_CPPFLAGS="$CPPFLAGS"
		save_LDFLAGS="$LDFLAGS"
		CPPFLAGS="$CPPFLAGS $NCURSES_CPPFLAGS"
		LDFLAGS="$LDFLAGS $NCURSES_LIBS"
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
				NCURSES_LIBS="$NCURSES_LIBS -lncurses $ASPELL_LIBS"
				have_ncurses="yes"
			])
		fi

		$as_unset have_ncurses_h

		CPPFLAGS="$save_CPPFLAGS"
		LDFLAGS="$save_LDFLAGS"
	fi
])

