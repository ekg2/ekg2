dnl Autoconf macros for libgnutls
dnl $id$

# Modified for LIBGNUTLS -- nmav
# Configure paths for LIBGCRYPT
# Shamelessly stolen from the one of XDELTA by Owen Taylor
# Werner Koch   99-12-09

dnl AM_PATH_LIBGNUTLS([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND ]]])
dnl Test for libgnutls, and define LIBGNUTLS_CFLAGS and LIBGNUTLS_LIBS
dnl
AC_DEFUN([AM_PATH_LIBGNUTLS],
[dnl
dnl Get the cflags and libraries from pkg-config
dnl

  pkg_config_args=gnutls
  no_libgnutls=""

  AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

  if test x$PKG_CONFIG != xno ; then
    if pkg-config --atleast-pkgconfig-version 0.7 ; then
      :
    else
      echo "*** pkg-config too old; version 0.7 or better required."
      no_libgnutls=yes
      PKG_CONFIG=no
    fi
  else
    no_libgnutls=yes
  fi

  min_libgnutls_version=ifelse([$1], ,0.1.0,$1)
  AC_MSG_CHECKING(for libgnutls - version >= $min_libgnutls_version)

  if test x$PKG_CONFIG != xno ; then
    if $PKG_CONFIG --exists $pkg_config_args; then
      :
    else
      no_libgnutls=yes
    fi
  fi

  if test x"$no_libgnutls" = x ; then
    LIBGNUTLS_CFLAGS=`$PKG_CONFIG --cflags $pkg_config_args`
    LIBGNUTLS_LIBS=`$PKG_CONFIG --libs $pkg_config_args`
    libgnutls_config_version=`$PKG_CONFIG --modversion $pkg_config_args`

    AC_DEFINE([HAVE_GNUTLS], 1, [define if you have GnuTLS])
    have_gnutls="yes"

    ac_save_CFLAGS="$CFLAGS"
    ac_save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $LIBGNUTLS_CFLAGS"
    LIBS="$LIBS $LIBGNUTLS_LIBS"
dnl
dnl Now check if the installed libgnutls is sufficiently new.
dnl
      rm -f conf.libgnutlstest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gnutls/gnutls.h>

int
main ()
{
    system ("touch conf.libgnutlstest");

    if( strcmp( gnutls_check_version(NULL), "$libgnutls_config_version" ) )
    {
      printf("\n*** 'pkg-config --modversion' returned %s, but LIBGNUTLS (%s)  was found!\n",
             "$libgnutls_config_version", gnutls_check_version(NULL) );
      printf("*** Remove the old version of LIBGNUTLS. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
    }
    else if ( strcmp(gnutls_check_version(NULL), LIBGNUTLS_VERSION ) )
    {
      printf("\n*** LIBGNUTLS header file (version %s) does not match\n", LIBGNUTLS_VERSION);
      printf("*** library (version %s)\n", gnutls_check_version(NULL) );
    }
    else
    {
      if ( gnutls_check_version( "$min_libgnutls_version" ) )
      {
        return 0;
      }
     else
      {
        printf("no\n*** An old version of LIBGNUTLS (%s) was found.\n",
                gnutls_check_version(NULL) );
        printf("*** You need a version of LIBGNUTLS newer than %s. The latest version of\n",
               "$min_libgnutls_version" );
        printf("*** LIBGNUTLS is always available from ftp://gnutls.hellug.gr/pub/gnutls.\n");
      }
    }
  return 1;
}
],, no_libgnutls=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
  fi

  if test "x$no_libgnutls" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])
  else
     if test -f conf.libgnutlstest ; then
        :
     else
        AC_MSG_RESULT(no)
        echo "*** Could not run libgnutls test program, checking why..."
	ac_save_CFLAGS="$CFLAGS"
	ac_save_LIBS="$LIBS"
        CFLAGS="$CFLAGS $LIBGNUTLS_CFLAGS"
        LIBS="$LIBS $LIBGNUTLS_LIBS"
        AC_TRY_LINK([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gnutls/gnutls.h>
],      [ return !!gnutls_check_version(NULL); ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding LIBGNUTLS or finding the wrong"
          echo "*** version of LIBGNUTLS. If it is not finding LIBGNUTLS, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
          echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"
          echo "***" ],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means LIBGNUTLS was incorrectly installed"
          echo "*** or that you have moved LIBGNUTLS since it was installed." ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
     fi

     LIBGNUTLS_CFLAGS=""
     LIBGNUTLS_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  rm -f conf.libgnutlstest
  AC_SUBST(LIBGNUTLS_CFLAGS)
  AC_SUBST(LIBGNUTLS_LIBS)
])

dnl *-*wedit:notab*-*  Please keep this as the last line.
