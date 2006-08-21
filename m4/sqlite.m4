dnl -*- autoconf -*-
dnl $id$
dnl Check for libsqlite, based on version found at libdbi-drivers.sf.net (GPLv2-licensed)

AC_DEFUN([AC_FIND_FILE], [
  $3=no
  for i in $2; do
      for j in $1; do
          if test -r "$i/$j"; then
              $3=$i
              break 2
          fi
      done
  done ])

AC_DEFUN([AC_CHECK_SQLITE], [
  have_sqlite="no"
  ac_sqlite="no"
  ac_sqlite_incdir="no"
  ac_sqlite_libdir="no"

  # exported variables
  SQLITE_LIBS=""
  SQLITE_CFLAGS=""

  AC_ARG_WITH(sqlite,		AC_HELP_STRING([--with-sqlite[=dir]],	[Compile with libsqlite at given dir]),
      [ ac_sqlite="$withval" 
        if test "x$withval" != "xno" -a test "x$withval" != "xyes"; then
            ac_sqlite="yes"
            ac_sqlite_incdir="$withval"/include
            ac_sqlite_libdir="$withval"/lib
        fi ],
      [ ac_sqlite="auto" ] )
  AC_ARG_WITH(sqlite-incdir,	AC_HELP_STRING([--with-sqlite-incdir],	[Specifies where the SQLite include files are.]),
      [  ac_sqlite_incdir="$withval" ] )
  AC_ARG_WITH(sqlite-libdir,	AC_HELP_STRING([--with-sqlite-libdir],	[Specifies where the SQLite libraries are.]),
      [  ac_sqlite_libdir="$withval" ] )

  # Try to automagically find SQLite, either with pkg-config, or without.
  if test "x$ac_sqlite" = "xauto"; then
      if test "x$PKGCONFIG" != "xno"; then
          AC_MSG_CHECKING([for SQLite])
          SQLITE_LIBS=$($PKGCONFIG --libs sqlite)
          SQLITE_CFLAGS=$($PKGCONFIG --cflags sqlite)
          if test "x$SQLITE_LIBS" = "x" -a "x$SQLITE_CFLAGS" = "x"; then
	      AC_CHECK_LIB([sqlite], [sqlite_open], [ac_sqlite="yes"], [ac_sqlite="no"])
	  else
              ac_sqlite="yes"
          fi
          AC_MSG_RESULT([$ac_sqlite])
      else
          AC_CHECK_LIB([sqlite], [sqlite_open], [ac_sqlite="yes"], [ac_sqlite="no"])
      fi
  fi

  if test "x$ac_sqlite" = "xyes"; then
      if test "$ac_sqlite_incdir" = "no" || test "$ac_sqlite_libs" = "no"; then
          sqlite_incdirs="/usr/include /usr/local/include /usr/include/sqlite /usr/local/include/sqlite /usr/local/sqlite/include /opt/sqlite/include"
          AC_FIND_FILE(sqlite.h, $sqlite_incdirs, ac_sqlite_incdir)
          sqlite_libdirs="/usr/lib /usr/local/lib /usr/lib/sqlite /usr/local/lib/sqlite /usr/local/sqlite/lib /opt/sqlite/lib"
          sqlite_libs="libsqlite.so libsqlite.a"
          AC_FIND_FILE($sqlite_libs, $sqlite_libdirs, ac_sqlite_libdir)
          if test "$ac_sqlite_incdir" = "no"; then
              AC_MSG_ERROR([Invalid SQLite directory - include files not found.])
          fi
          if test "$ac_sqlite_libdir" = "no"; then
              AC_MSG_ERROR([Invalid SQLite directory - libraries not found.])
          fi
      fi
      have_sqlite="yes"

      test "x$SQLITE_LIBS" = "x" && SQLITE_LIBS="-L$ac_sqlite_libdir -lsqlite"
      test "x$SQLITE_CFLAGS" = "x" && SQLITE_INCLUDE=-I$ac_sqlite_incdir

      AC_SUBST(SQLITE_LIBS)
      AC_SUBST(SQLITE_CFLAGS)
  fi
])
