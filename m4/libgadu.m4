dnl based on openssl.m4 
dnl $Id$

AC_DEFUN(AC_CHECK_LIBGADU,[
  AC_SUBST(LIBGADU_LIBS)
  AC_SUBST(LIBGADU_INCLUDES)

  AC_ARG_WITH(LIBGADU,
    [[  --without-libgadu       Compile without libgadu]], 
      if test "x$withval" = "xno" ; then
        without_libgadu=yes
      elif test "x$withval" != "xyes" ; then
        with_arg=$withval/include:-L$withval/lib
      fi)

  if test "x$without_libgadu" != "xyes" -a "x$with_arg" = "x"; then
    AC_CHECK_PROG([PKGCONFIG], [pkg-config], [pkg-config], [no])
    if test "x$PKGCONFIG" != "xno"; then
      AC_MSG_CHECKING([for libgadu])
      LIBGADU_LIBS=$($PKGCONFIG --libs libgadu)
      LIBGADU_INCLUDES=$($PKGCONFIG --cflags libgadu)
      if test "x$LIBGADU_LIBS" != "x" -o "x$LIBGADU_INCLUDES" != "x"; then
	AC_DEFINE(HAVE_LIBGADU, 1, [define if you have libgadu])
	AC_MSG_RESULT([yes])
        without_libgadu=yes
	have_libgadu=yes
      else
        AC_MSG_RESULT([no])
      fi
    fi
  fi

  if test "x$without_libgadu" != "xyes" ; then
    AC_MSG_CHECKING(for libgadu.h)

    for i in $with_arg \
    		/usr/include: \
		/usr/local/include:"-L/usr/local/lib" \
		/usr/pkg/include:"-L/usr/pkg/lib" \
		/usr/contrib/include:"-L/usr/contrib/lib" \
		/usr/freeware/include:"-L/usr/freeware/lib32" \
    		/sw/include:"-L/sw/lib" \
    		/cw/include:"-L/cw/lib" \
		/boot/home/config/include:"-L/boot/home/config/lib"; do
	
      incl=`echo "$i" | sed 's/:.*//'`
      lib=`echo "$i" | sed 's/.*://'`

      if test -f $incl/libgadu.h; then
        AC_MSG_RESULT($incl/libgadu.h)
	ldflags_old="$LDFLAGS"
	LDFLAGS="$lib"
	save_LIBS="$LIBS"
	LIBS="$LIBS"
	AC_CHECK_LIB(libgadu, gg_login, [
	  AC_DEFINE(HAVE_LIBGADU, 1, [define if you have libgadu])
	  have_libgadu=yes
	  LIBGADU_LIBS="$lib -lgadu -lcrypto"
	  if test "x$incl" != "x/usr/include"; then
    	    LIBGADU_INCLUDES="-I$incl"
	  fi
	])
	LIBS="$save_LIBS"
	LDFLAGS="$ldflags_old"
	break
      fi
    done

    if test "x$have_libgadu" != "xyes"; then
      AC_MSG_RESULT(not found)
    fi
  fi
])
