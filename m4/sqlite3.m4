dnl -*- autoconf -*-
dnl $id$
dnl Check for libsqlite3, based on version found at libdbi-drivers.sf.net (GPLv2-licensed)

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

AC_DEFUN([AC_CHECK_SQLITE3], [
  have_sqlite3="no"
  ac_sqlite3="no"
  ac_sqlite3_incdir="no"
  ac_sqlite3_libdir="no"

  # exported variables
  SQLITE_LIBS=""
  SQLITE_CFLAGS=""

  AC_ARG_WITH(sqlite3,
      AC_HELP_STRING( [--with-sqlite3[=dir]] , [Compile with libsqlite3 at given dir] ),
      [ ac_sqlite3="$withval" 
        if test "x$withval" != "xno" -a "x$withval" != "xyes"; then
            ac_sqlite3="yes"
            ac_sqlite3_incdir="$withval"/include
            ac_sqlite3_libdir="$withval"/lib
        fi ],
      [ ac_sqlite3="auto" ] )
  AC_ARG_WITH(sqlite3-incdir,
      AC_HELP_STRING( [--with-sqlite3-incdir],
                      [Specifies where the SQLite3 include files are.] ),
      [  ac_sqlite3_incdir="$withval" ] )
  AC_ARG_WITH(sqlite3-libdir,
      AC_HELP_STRING( [--with-sqlite3-libdir],
                      [Specifies where the SQLite3 libraries are.] ),
      [  ac_sqlite3_libdir="$withval" ] )

  # Try to automagically find SQLite, either with pkg-config, or without.
  if test "x$ac_sqlite3" = "xauto"; then
      if test "x$PKGCONFIG" != "xno"; then
          AC_MSG_CHECKING([for SQLite3])
          SQLITE_LIBS=$($PKGCONFIG --libs sqlite3)
          SQLITE_CFLAGS=$($PKGCONFIG --cflags sqlite3)
          if test "x$SQLITE_LIBS" = "x" -a "x$SQLITE_CFLAGS" = "x"; then
	      AC_CHECK_LIB([sqlite3], [sqlite3_open], [ac_sqlite3="yes"], [ac_sqlite3="no"])
	  else
              ac_sqlite3="yes"
          fi
          AC_MSG_RESULT([$ac_sqlite3])
      else
          AC_CHECK_LIB([sqlite3], [sqlite3_open], [ac_sqlite3="yes"], [ac_sqlite3="no"])
      fi
  fi

  if test "x$ac_sqlite3" = "xyes"; then
      if test "$ac_sqlite3$_incdir" = "no" || test "$ac_sqlite333_libs" = "no"; then
          sqlite3$_incdirs="/usr/include /usr/local/include /usr/include/sqlite /usr/local/include/sqlite /usr/local/sqlite/include /opt/sqlite/include"
          AC_FIND_FILE(sqlite3.h, $sqlite3$_incdirs, ac_sqlite3_incdir)
          sqlite3_libdirs="/usr/lib /usr/local/lib /usr/lib/sqlite /usr/local/lib/sqlite /usr/local/sqlite/lib /opt/sqlite/lib"
          sqlite3_libs="libsqlite3.so libsqlite3.a"
          AC_FIND_FILE($sqlite3_libs, $sqlite3_libdirs, ac_sqlite3_libdir)
          if test "$ac_sqlite3_incdir" = "no"; then
              AC_MSG_ERROR([Invalid SQLite directory - include files not found.])
          fi
          if test "$ac_sqlite3_libdir" = "no"; then
              AC_MSG_ERROR([Invalid SQLite directory - libraries not found.])
          fi
      fi
      have_sqlite3="yes"

      if test x"$ac_sqlite3_libdir" = xno; then
          test "x$SQLITE_LIBS" = "x" && SQLITE_LIBS="-lsqlite3"
      else
          test "x$SQLITE_LIBS" = "x" && SQLITE_LIBS="-L$ac_sqlite3_libdir -lsqlite3"
      fi
      test x"$ac_sqlite3_incdir" = xno && test "x$SQLITE_CFLAGS" = "x" && SQLITE_INCLUDE=-I$ac_sqlite3_incdir

      AC_SUBST(SQLITE_LIBS)
      AC_SUBST(SQLITE_CFLAGS)
  fi
])
