
AC_DEFUN([AC_CHECK_ORACLE],
[

        dnl $have_pthread is defined by 'resolver libgadu'
        if test "x$have_pthread" == "xyes"; then
                if test -z "$ORACLE_HOME"; then
                        AC_MSG_WARN([logsoracle plugin requires enviroment variable ORACLE_HOME to be set (incomplete oracle installation?)]);
                else
                        AC_SUBST(LOGS_ORACLE_LIBS)
                        AC_SUBST(LOGS_ORACLE_CFLAGS)
                        LOGS_ORACLE_LIBS="-L${ORACLE_HOME}/lib"
                        LOGS_ORACLE_CFLAGS="-I${ORACLE_HOME}/rdbms/public"

                        save_LIBS="$LIBS";
                        save_CFLAGS="$CFLAGS";
                        LIBS="$LIBS ${LOGS_ORACLE_LIBS}"
                        CFLAGS="$CFLAGS ${LOGS_ORACLE_CFLAGS}"

                        AC_CHECK_LIB(clntsh, sqlcxt,[have_clntsh=yes],[have_clntsh=no])

                        if test "x$have_clntsh" == "xyes"; then
                                AC_CHECK_HEADER(oci.h,[have_oci_h=yes],[have_oci_h=no])
                                if test "x$have_oci_h" != "xyes"; then
                                        AC_MSG_WARN([logsoracle plugin : oracle headers not found (oci.h)]);
                                else
                                        PLUGINS="$PLUGINS logsoracle"
                                        AC_MSG_RESULT([logsoracle plugin : added to plugin list ])

                                        have_logsoracle=yes;
                                fi
                        else
                                AC_MSG_WARN([logsoracle plugin : couldn't find libclntsh]);
                        fi

                        LIBS="$save_LIBS";
                        CFLAGS="$save_CFLAGS";
                fi
                else
                        AC_MSG_WARN([logsoracle plugin requires pthreads to work])
        fi


])

