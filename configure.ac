AC_INIT([tlsdate],[0.0.1],[jacob at appelbaum.net])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_TARGET
AC_ARG_PROGRAM
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([-Wall -Werror -Wno-portability subdir-objects foreign tar-ustar])

AC_PREREQ([2.63])

AC_CONFIG_HEADERS([config.h:config.in])dnl Keep filename to 8.3 for MS-DOS.

LT_PREREQ([2.2])
LT_INIT
LT_LANG([C])
gl_VISIBILITY
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

CONFIG_EXTRA

dnl Here we should build a small program to fetch the build system time in a portable
dnl manner. We have no Win32 users, we can fix this if we ever find one that
dnl cares.
COMPILE_DATE=`date +%s`
AC_SUBST([COMPILE_DATE])
AC_DEFINE_UNQUOTED([RECENT_COMPILE_DATE],
                   [(uint32_t) ${COMPILE_DATE}],
                   [Time in seconds since the Disco epoch at build time])

# Required headers
# First check to see if openssl is installed
AC_CHECK_HEADERS([openssl/ssl.h], ,[AC_MSG_ERROR([OpenSSL is not installed, openssl/sslh is missing])])

AC_CHECK_HEADERS([arpa/inet.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([getopt.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([grp.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([openssl/bio.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([openssl/err.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([openssl/evp.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([pwd.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([stdint.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([stdio.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([stdlib.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([sys/mman.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([sys/time.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([sys/types.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([sys/wait.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([time.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([unistd.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])

AC_CHECK_FUNCS([setresuid])
AC_CHECK_FUNCS([gettimeofday])

# Check for -lrt
AC_CHECK_LIB([rt], [clock_gettime], [
              RT_LIB="-lrt"
              AC_SUBST(RT_LIB)
              AC_DEFINE([HAVE_LIBRT], [1], [Have clock_gettime])
              ],[
                 AC_MSG_ERROR([Your system lacks clock_gettime])
              ])

# Debug and hardening flags all in one shot
# Alwas do this at the end, otherwise you end up filtering system/other libraries
AC_DEFUN([LOCAL_CHECK_FLAGS],[
          AC_REQUIRE([AX_CHECK_LINK_FLAG])
          AC_REQUIRE([AX_APPEND_COMPILE_FLAGS])
          AC_LANG_PUSH([C])
          CFLAGS=
          LIBS=
          AX_APPEND_COMPILE_FLAGS([-g -O1 -Wall])
          AX_APPEND_COMPILE_FLAGS([-fno-strict-aliasing])
          AX_APPEND_COMPILE_FLAGS([-D_FORTIFY_SOURCE=2 -fstack-protector-all])
          AX_APPEND_COMPILE_FLAGS([-fwrapv -fPIE -Wstack-protector])
          AX_APPEND_COMPILE_FLAGS([--param ssp-buffer-size=1])
          AX_CHECK_LINK_FLAG([-pie -z relro -z now])
          AC_LANG_POP
          ])
LOCAL_CHECK_FLAGS

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
