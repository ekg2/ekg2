dnl $Id$

AC_DEFUN([AC_CHECK_EXPAT],
[
	AC_SUBST(EXPAT_LIBS)
	AC_SUBST(EXPAT_CPPFLAGS)

	AC_ARG_WITH(expat, [[  --with-expat[=dir]    Compile with expat/locate base dir]], [
		if test "x$withval" = "xno"; then
			without_expat=yes
		elif test "x$withval" != "xyes"; then
			EXPAT_CPPFLAGS="-I$withval/include"
			EXPAT_LIBS="-L$withval/lib"
		fi
	])

	if test "x$without_expat" != "xyes"; then
		cf="$CPPFLAGS"
		lf="$LDFLAGS"
		CPPFLAGS="$CPPFLAGS $EXPAT_CPPFLAGS"
		LDFLAGS="$LDFLAGS $EXPAT_LIBS"
		AC_CHECK_HEADERS([expat.h],
		[
			AC_CHECK_LIB([expat], [XML_ParserCreate],
			[
				AC_DEFINE([HAVE_EXPAT], 1, [define if you have expat])
				EXPAT_LIBS="$EXPAT_LIBS -lexpat"
				have_expat="yes"
			])
		])
		CPPFLAGS="$cf"
		LDFLAGS="$lf"
	fi
])

