AC_DEFUN([AC_CHECK_LIBGADU],
[
	AC_SUBST(LIBGADU_LIBS)
	AC_SUBST(LIBGADU_CPPFLAGS)

	AC_ARG_WITH(libgadu, [[  --with-libgadu[=dir]    Compile with libgadu/locate base dir]], [
		if test "x$withval" = "xno"; then
			without_libgadu=yes
		elif test "x$withval" != "xyes"; then
			LIBGADU_CPPFLAGS="-I$withval/include"
			LIBGADU_LIBS="-L$withval/lib -lgadu -lcrypto -lssl $PTHREAD_LIBS"
		fi
	])

## TOOD: convert to PKG_CONFIG, pkg-config might know better (mp)
	if test "x$without_libgadu" != "xyes"; then
		ocf="$CPPFLAGS"
		olf="$LDFLAGS"
		CPPFLAGS="$CPPFLAGS $LIBGADU_CPPFLAGS"
		LDFLAGS="$LDFLAGS $LIBGADU_LIBS"
		save_LIBS="$LIBS"
		LIBS="-lgadu -lcrypto -lssl $LIBS $PTHREAD_LIBS"
		AC_CHECK_HEADERS([libgadu.h],
		[
			AC_CHECK_LIB([gadu], [gg_logoff],
			[
				AC_DEFINE([HAVE_LIBGADU], 1, [define if you have libgadu])
				LIBGADU_LIBS="$LIBGADU_LIBS -lgadu -lcrypto -lssl $PTHREAD_LIBS $LIBJPEG_LIBS"
				have_libgadu="yes"
			])
		])
		LIBS="$save_LIBS"
		CPPFLAGS="$ocf"
		LDFLAGS="$olf"
	fi
])

