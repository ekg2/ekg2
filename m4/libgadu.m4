AC_DEFUN([AC_CHECK_LIBGADU],
[
	AC_SUBST(LIBGADU_LIBS)
	AC_SUBST(LIBGADU_CPPFLAGS)

	AC_ARG_WITH(libgadu, [[  --with-libgadu[=dir]    Compile with libgadu/locate base dir]], [
		if test "x$withval" = "xno"; then
			without_libgadu=yes
		elif test "x$withval" != "xyes"; then
			LIBGADU_CPPFLAGS="-I$withval/include"
			LIBGADU_LIBS="-L$withval/lib"
		fi
	])

	if test "x$without_libgadu" != "xyes"; then
		CPPFLAGS="$CPPFLAGS $LIBGADU_CPPFLAGS"
		LDFLAGS="$LDFLAGS $LIBGADU_LIBS"
		AC_CHECK_HEADERS([libgadu.h],
		[
			AC_CHECK_LIB([gadu], [gg_login],
			[
				AC_DEFINE([HAVE_LIBGADU], 1, [define if you have expat])
				EXPAT_LIBS="$LIBGADU_LIBS -lgadu"
				have_libgadu="yes"
			])
		])
	fi
])

