dnl (C) Copyright 2011 Marcin Owsiany <porridge@debian.org>
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License Version 2 as
dnl published by the Free Software Foundation.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
AC_DEFUN([AC_PROG_C_NO_STRICT_ALIASING],
[
        AC_SUBST(C_STRICT_ALIASING)

        ac_save_CFLAGS=$CFLAGS
        CFLAGS="-fno-strict-aliasing"

        AC_CACHE_CHECK([whether $CC accepts ${CFLAGS}], [ekg2_cv_no_strict_aliasing_accepted],
            [AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
             [ekg2_cv_no_strict_aliasing_accepted=yes],
             [ekg2_cv_no_strict_aliasing_accepted=no]
            )]
        )
        if test "$ekg2_cv_no_strict_aliasing_accepted" = yes; then
            C_STRICT_ALIASING="${CFLAGS}"
        else
            C_STRICT_ALIASING=""
        fi

        CFLAGS=$ac_save_CFLAGS
])
