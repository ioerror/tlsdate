AC_INIT([tlsdate],[0.0.5],[jacob at appelbaum.net])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_TARGET
AC_ARG_PROGRAM
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([-Wall -Werror -Wno-portability subdir-objects foreign tar-ustar])

AC_PREREQ([2.63])

AC_CONFIG_HEADERS([config.h:config.in])dnl Keep filename to 8.3 for MS-DOS.

PKG_PROG_PKG_CONFIG
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
                   [${COMPILE_DATE}L],
                   [Time in seconds since the Disco epoch at build time])

dnl Build up the directory we will use to install certs
TLSDATE_CA_ROOTS="${sysconfdir}/$PACKAGE_NAME/ca-roots"
AC_SUBST([TLSDATE_CA_ROOTS])

dnl Place we install our config file
TLSDATE_CONF_DIR="${sysconfdir}/$PACKAGE_NAME/"
AC_SUBST([TLSDATE_CONF_DIR])

dnl Required headers
dnl First check to see if openssl is installed
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

AC_CHECK_FUNCS_ONCE(m4_flatten([
    gettimeofday
    prctl
    preadv
    pwritev
    setresuid
]))

AC_MSG_CHECKING([user/group to drop privs to])

AC_ARG_WITH([unpriv-user],
            [AS_HELP_STRING([--with-unpriv-user=<user>],
                            [User to drop privs to @<:@default: nobody@:>@])])
AS_CASE([$with_unpriv_user],
        [""|yes|no], [UNPRIV_USER="nobody"],
        [*], [UNPRIV_USER=$with_unpriv_user])
AC_DEFINE_UNQUOTED([UNPRIV_USER], ["${UNPRIV_USER}"], [Unprivileged user])
AC_SUBST([UNPRIV_USER])

AC_ARG_WITH([unpriv-group],
            [AS_HELP_STRING([--with-unpriv-group=<group>],
                            [Group to drop privs to @<:@default: nogroup@:>@])])
AS_CASE([$with_unpriv_group],
        [""|yes|no], [UNPRIV_GROUP="nogroup"],
        [*], [UNPRIV_GROUP=$with_unpriv_group])
AC_DEFINE_UNQUOTED([UNPRIV_GROUP], ["${UNPRIV_GROUP}"], [Unprivileged group])

AC_MSG_RESULT(${UNPRIV_USER}:${UNPRIV_GROUP})

AC_MSG_CHECKING([group to allow DBus calls from])
AC_ARG_WITH([dbus-client-group],
            [AS_HELP_STRING([--with-dbus-client-group=<group>],
                [Allow dbus method calls from group @<:@default: root@:>@])])
AS_CASE([$with_dbus_client_group],
        [""|yes|no], [DBUS_CLIENT_GROUP="root"],
        [*], [DBUS_CLIENT_GROUP=$with_dbus_client_group])
AC_DEFINE_UNQUOTED([DBUS_CLIENT_GROUP], ["${DBUS_CLIENT_GROUP}"], [DBus client group])
AC_MSG_RESULT(${DBUS_CLIENT_GROUP})
AC_SUBST([DBUS_CLIENT_GROUP])

dnl Check for clock_gettime.  Some systems put it into -lc, while
dnl others use -lrt.  Try the first and fallback to the latter.
RT_LIB=
AC_CHECK_FUNC([clock_gettime], [:],
              [AC_CHECK_LIB([rt], [clock_gettime], [RT_LIB="-lrt"],
                            [AC_MSG_ERROR([Your system lacks clock_gettime])])])
AC_SUBST(RT_LIB)

PKG_CHECK_MODULES([LIBEVENT], [libevent >= 2.0])

have_dbus=false
AC_ARG_ENABLE([dbus],
              [AS_HELP_STRING([--disable-dbus],
                              [Disable automatically dbus support])])
AS_IF([test "x$enable_dbus" = xyes], [
    PKG_CHECK_MODULES([DBUS], [dbus-1], [
            have_dbus=true
            AC_DEFINE([HAVE_DBUS], [1], [dbus enabled])
        ], [
            AS_IF([test "x$enable_dbus" = xyes],
                  [AC_MSG_ERROR([dbus requested but not found])])
        ])
    ])
AM_CONDITIONAL([HAVE_DBUS], ${have_dbus})

AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)
AC_SUBST(LIBEVENT_CFLAGS)
AC_SUBST(LIBEVENT_LIBS)

have_seccomp_filter=false
AC_ARG_ENABLE([seccomp_filter],
              [AS_HELP_STRING([--enable-seccomp-filter],
                              [Require seccomp filter])])

AC_MSG_CHECKING([kernel for seccomp_filter support])
AS_IF([test "x$enable_seccomp_filter" = xyes], [
    dnl Force seccomp filter use
    have_seccomp_filter=true
    AC_MSG_RESULT([forced])
  ], [
  AS_IF([test "x$enable_seccomp_filter" = xno], [
      have_seccomp_filter=no
      AC_MSG_RESULT([disabled])
  ], [
  dnl Detect seccomp filter support.
  AC_RUN_IFELSE([AC_LANG_PROGRAM([[
      #include <errno.h>
      #include <linux/audit.h>
      #include <linux/filter.h>
      #include <stdlib.h>
      #include <sys/prctl.h>
      #include "src/seccomp-compat.h"
    ]],
    [[ errno = 0;
       if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
         exit(1);
       prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, NULL, 0, 0);
       exit(errno == EFAULT ? 0 : 1); ]])],
    [ AC_MSG_RESULT([yes])
      have_seccomp_filter=true
    ], [
      AC_MSG_RESULT([no])
    ],
    [ AC_MSG_RESULT([cross-compiling, assuming yes])
      have_seccomp_filter=true
    ]
  )
])])

AS_IF([${have_seccomp_filter}], [
    AC_DEFINE([HAVE_SECCOMP_FILTER], [1], [Enable seccomp filter])
  ])
AM_CONDITIONAL([HAVE_SECCOMP_FILTER], ${have_seccomp_filter})



have_seccomp_debug=false
AC_ARG_ENABLE([seccomp_debugging],
              [AS_HELP_STRING([--enable-seccomp-debugging],
                [Enable seccomp filter debugging])])
AS_IF([test "x$enable_seccomp_debugging" = xyes], [
    AC_DEFINE([SECCOMP_FILTER_DEBUG], [1], [Enable seccomp filter debugging])
    have_seccomp_debug=true
  ])
AM_CONDITIONAL([SECCOMP_FILTER_DEBUG], ${have_seccomp_debug})


AC_MSG_CHECKING([for CrOS-specific platform wake event support])
AC_ARG_ENABLE([cros],
              [AS_HELP_STRING([--disable-cros],
                              [Disable CrOS platform support])])

AS_IF([test "x$enable_cros" = xyes -a "x$enable_dbus" != xyes ], [
    AC_MSG_ERROR([--enable-dbus is required for --enable-cros])
  ])

have_cros=false
AS_IF([test "x$enable_cros" = xyes], [
    have_cros=true
    AC_DEFINE([HAVE_CROS], [1], [Enable CrOS support])
    AC_MSG_RESULT([yes])
  ], [
    AC_MSG_RESULT([no])
  ])
AM_CONDITIONAL([HAVE_CROS], ${have_cros})

dnl Debug and hardening flags all in one shot
dnl Always do this at the end, otherwise you end up filtering system/other libraries
AC_ARG_ENABLE([hardened-checks],
              [AS_HELP_STRING([--disable-hardened-checks],
                              [Disable automatically enabling hardened toolchain options])])
AC_DEFUN([LOCAL_CHECK_FLAGS],[
          AC_REQUIRE([AX_CHECK_LINK_FLAG])
          AC_REQUIRE([AX_APPEND_COMPILE_FLAGS])
          AC_LANG_PUSH([C])
          AS_IF([test "x$enable_hardened_checks" != xno], [
              CFLAGS=
              LIBS=
              AX_APPEND_COMPILE_FLAGS([-g -O1])
          ], [
              AC_MSG_WARN([using hardened flags is HIGHLY RECOMMENDED and disabling them is a BAD IDEA])
          ])
          AX_APPEND_COMPILE_FLAGS([-Wall -fno-strict-aliasing])
          AS_IF([test "x$enable_hardened_checks" != xno], [
              AX_APPEND_COMPILE_FLAGS([-D_FORTIFY_SOURCE=2 -fstack-protector-all])
              AX_APPEND_COMPILE_FLAGS([-fwrapv -fPIE -Wstack-protector])
              AX_APPEND_COMPILE_FLAGS([--param=ssp-buffer-size=1])
              AX_CHECK_LINK_FLAG([-z relro -z now])
              AX_CHECK_LINK_FLAG([-pie])
          ])
          AC_LANG_POP
          ])
LOCAL_CHECK_FLAGS

AC_ARG_ENABLE([code-coverage-checks],
              [AS_HELP_STRING([--enable-code-coverage-checks],
                              [Enable gcov/lcov compile time options])],
              [AX_APPEND_COMPILE_FLAGS([-ftest-coverage -fprofile-arcs])])

AC_CONFIG_FILES([dbus/org.torproject.tlsdate.conf])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
