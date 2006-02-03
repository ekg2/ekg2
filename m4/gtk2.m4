# oparte na libgadu.m4
# brzydkie i w ogole
# i w sumie nie wiem po co napisalem.. w koncu w snapach i tak tego nie bedzie, nie?
# dj.

AC_DEFUN([AC_CHECK_GTK2],
[
  AC_SUBST([GTK2_LIBS])
  AC_SUBST([GTK2_CFLAGS])
  GTK2_LIBS=
  GTK2_CFLAGS=
  

  AC_ARG_WITH([gtk],
    AC_HELP_STRING([--with-gtk], [use gtk+-2.0]),
      if test "x$withval" = "xno" ; then
        without_gtk=yes
      elif test "x$withval" != "xyes" ; then
        with_arg="-I$withval/include:-L$withval/lib"
      fi)

  if test "x$without_gtk" != "xyes" -a "x$with_arg" = "x"; then
    ## for now, we'll check for pkg-config idenpendently - it's cached anyway
    if test "x$PKGCONFIG" != "xno"; then
      AC_MSG_CHECKING([for gtk+-2.0])
      if $PKGCONFIG --exists gtk+-2.0; then
        GTK2_LIBS=$($PKGCONFIG --libs gtk+-2.0)
        GTK2_CFLAGS=$($PKGCONFIG --cflags gtk+-2.0)
        AC_DEFINE(HAVE_GTK, 1, [define if you have GTK+-2.0])
        AC_MSG_RESULT([yes (found via pkg-config)])
      else
        ## pkg-config nie znalazl... wiêc sprawdzamy standardowo metod±
        AC_MSG_WARN([unknown to pkg-config. use --with-gtk=DIR if libgadu is installed in nonstandard location])
      fi
    fi
  elif test "x$without_gtk" != "xyes" -a "x$with_arg" != "x"; then
    AC_MSG_NOTICE("gtk2 directory specified by user: $with_arg")
    # TODO.
  fi
  if test "x$without_libgadu" != "xyes"; then  	
  	# sprawdzic czy sie kompiluje? / jakies bibloteki ? cokolwiek ?
  	have_gtk="yes"
  fi

])

