dnl Rewritten from scratch. --wojtekka
dnl $Id$

AC_DEFUN([AC_CHECK_READLINE],[
  AC_SUBST(READLINE_LIBS)
  AC_SUBST(READLINE_CFLAGS)

  AC_ARG_WITH(readline,
    [[  --with-readline[=dir]   Compile with readline/locate base dir]],
    if test "x$withval" = "xno" ; then
      without_readline=yes
    elif test "x$withval" != "xyes" ; then
      with_arg="$withval/include:-L$withval/lib $withval/include/readline:-L$withval/lib"
    fi)

  AC_MSG_CHECKING(for readline.h)

  if test "x$without_readline" != "xyes"; then
    for i in $with_arg \
	     /usr/include: \
	     /usr/local/include:-L/usr/local/lib \
             /usr/freeware/include:-L/usr/freeware/lib32 \
	     /usr/pkg/include:-L/usr/pkg/lib \
	     /sw/include:-L/sw/lib \
	     /cw/include:-L/cw/lib \
	     /net/caladium/usr/people/piotr.nba/temp/pkg/include:-L/net/caladium/usr/people/piotr.nba/temp/pkg/lib \
	     /boot/home/config/include:-L/boot/home/config/lib; do
    
      incl=`echo "$i" | sed 's/:.*//'`
      lib=`echo "$i" | sed 's/.*://'`

      if test -f $incl/readline/readline.h ; then
        AC_MSG_RESULT($incl/readline/readline.h)
        READLINE_LIBS="$lib -lreadline"
	if test "$incl" != "/usr/include"; then
	  READLINE_CFLAGS="-I$incl/readline -I$incl"
	else
	  READLINE_CFLAGS="-I$incl/readline"
	fi
        AC_DEFINE(HAVE_READLINE, 1, [define if You want readline])
        have_readline=yes
        break
      elif test -f $incl/readline.h -a "x$incl" != "x/usr/include"; then
        AC_MSG_RESULT($incl/readline.h)
        READLINE_LIBS="$lib -lreadline"
        READLINE_CFLAGS="-I$incl"
        AC_DEFINE(HAVE_READLINE, 1, [define if You want readline])
        have_readline=yes
        break
      fi
    done
  fi

  if test "x$have_readline" != "xyes"; then
    AC_MSG_RESULT(not found)
  fi
])

