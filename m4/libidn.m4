dnl copied as-is from 'info libidn'

AC_SUBST(IDN_LIBS)

AC_DEFUN([AC_CHECK_LIBIDN], [
     AC_ARG_WITH(libidn, AC_HELP_STRING([--with-libidn=[DIR]],
                                     [Support IDN (needs GNU Libidn)]),
       libidn=$withval, libidn=yes)
     if test "$libidn" != "no"; then
       if test "$libidn" != "yes"; then
         LDFLAGS="${LDFLAGS} -L$libidn/lib"
         CPPFLAGS="${CPPFLAGS} -I$libidn/include"
       fi
       AC_CHECK_HEADER(idna.h,
         AC_CHECK_LIB(idn, stringprep_check_version,
           [libidn=yes IDN_LIBS="-lidn"], libidn=no),
         libidn=no)
     fi
     if test "$libidn" != "no" ; then
       AC_DEFINE(LIBIDN, 1, [Define to 1 if you want IDN support.])
     else
       AC_MSG_WARN([Libidn not found])
     fi
     AC_MSG_CHECKING([if Libidn should be used])
     AC_MSG_RESULT($libidn)
])
