dnl $Id$

AC_DEFUN([AC_CHECK_LIBGSM],
[
	AC_SUBST(LIBGSM_LIBS)
	AC_SUBST(LIBGSM_CFLAGS)

	AC_ARG_WITH(libgsm, [[  --with-libgsm[=dir]   Compile with libgsm/locate base dir]], [
		if test "x$withval" = "xno"; then
			without_libgsm=yes
		elif test "x$withval" != "xyes"; then
			LIBGSM_CFLAGS="-I$withval/include"
			LIBGSM_LIBS="-L$withval/lib"
		fi
	])

	if test "x$without_libgsm" != "xyes"; then
		old_cppflgs="$CPPFLAGS"
		old_ldflags="$LDFLAGS"
		CPPFLAGS="$CPPFLAGS $LIBGSM_CFLAGS"
		LDFLAGS="$LDFLAGS $LIBGSM_LIBS"
		have_libgsm_h=""
		AC_CHECK_HEADERS([gsm.h],
		[
			have_libgsm_h="yes"
		], [
			AC_CHECK_HEADERS([libgsm/gsm.h],
			[
				have_libgsm_h="yes"
			])
		])

		if test "x$have_libgsm_h" = "xyes"; then
			AC_CHECK_LIB([gsm], [gsm_create],
			[
				AC_DEFINE([HAVE_LIBGSM], 1, [define if you have libgsm])
				LIBGSM_LIBS="$LIBGSM_LIBS -lgsm"
				have_libgsm="yes"
			])
		fi

		$as_unset have_libgsm_h

		CPPFLAGS="$old_cppflags"
		LDFLAGS="$old_ldflags"
	fi
])

