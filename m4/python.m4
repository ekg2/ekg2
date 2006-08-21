dnl Rewritten from scratch. --speedy 
dnl Little bit fixed. --leafnode
dnl $Id$

PYTHON=
PYTHON_VERSION=
PYTHON_CFLAGS=
PYTHON_LIBS=

AC_DEFUN([AM_CHECK_PYTHON],
[
	AC_SUBST(PYTHON_LIBS)
	AC_SUBST(PYTHON_CFLAGS)

	AC_ARG_WITH(python,		AC_HELP_STRING([--with-python],		[Compile with Python bindings]),
		if test "x$withval" != "xno" -a "x$withval" != "xyes"; then
			ith_arg="$withval/include:-L$withval/lib $withval/include/python:-L$withval/lib"
		fi
	)

	if test "x$with_python" != "xno"; then

		AC_PATH_PROG(PYTHON, python)
		if test "$PYTHON" = ""; then AC_PATH_PROG(PYTHON, python2.3) fi
		if test "$PYTHON" = ""; then AC_PATH_PROG(PYTHON, python2.2) fi
		if test "$PYTHON" = ""; then AC_PATH_PROG(PYTHON, python2.1) fi
		if test "$PYTHON" = ""; then AC_PATH_PROG(PYTHON, python2.0) fi

		if test "$PYTHON" != ""; then
			PY_VERSION=`$PYTHON -c "import sys; print sys.version[[0:3]]"`
			PY_PREFIX=`$PYTHON -c "import sys; print sys.prefix"`
			echo "Found Python version $PY_VERSION [$PY_PREFIX]"
		fi

		AC_MSG_CHECKING(for Python.h)

		PY_EXEC_PREFIX=`$PYTHON -c "import sys; print sys.exec_prefix"`

		if test "$PY_VERSION" != ""; then 
			if test -f $PY_PREFIX/include/python$PY_VERSION/Python.h ; then 
			AC_MSG_RESULT($PY_PREFIX/include/python$PY_VERSION/Python.h)
				PY_LIB_LOC="-L$PY_EXEC_PREFIX/lib/python$PY_VERSION/config"
				PY_CFLAGS="-I$PY_PREFIX/include/python$PY_VERSION"
				PY_MAKEFILE="$PY_EXEC_PREFIX/lib/python$PY_VERSION/config/Makefile"

				PY_LOCALMODLIBS=`sed -n -e 's/^LOCALMODLIBS=\(.*\)/\1/p' $PY_MAKEFILE`
				PY_BASEMODLIBS=`sed -n -e 's/^BASEMODLIBS=\(.*\)/\1/p' $PY_MAKEFILE`
				PY_OTHER_LIBS=`sed -n -e 's/^LIBS=\(.*\)/\1/p' $PY_MAKEFILE`
				PY_OTHER_LIBM=`sed -n -e 's/^LIBC=\(.*\)/\1/p' $PY_MAKEFILE`
				PY_OTHER_LIBC=`sed -n -e 's/^LIBM=\(.*\)/\1/p' $PY_MAKEFILE`
				PY_LIBS="$PY_LOCALMODLIBS $PY_BASEMODLIBS $PY_OTHER_LIBS $PY_OTHER_LIBC $PY_OTHER_LIBM"

				PYTHON_LIBS="-L$PY_EXEC_PREFIX/lib $PY_LIB_LOC -lpython$PY_VERSION $PY_LIBS"
				PYTHON_CFLAGS="$PY_CFLAGS"
				AC_DEFINE(WITH_PYTHON, 1, [define if You want python])
				have_python=yes
			fi
		fi

		if test "x$have_python" != "xyes"; then 
			AC_MSG_RESULT(not found)
		fi
	fi
])
