AC_INIT([tlsdate],[0.0.6],[jacob at appelbaum.net])
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
AX_PLATFORM

dnl Here we should build a small program to fetch the build system time in a portable
dnl manner. We have no Win32 users, we can fix this if we ever find one that
dnl cares.
COMPILE_DATE=`date +%s`
AC_SUBST([COMPILE_DATE])
AC_DEFINE_UNQUOTED([RECENT_COMPILE_DATE],
                   [(uint32_t) ${COMPILE_DATE}],
                   [Time in seconds since the Disco epoch at build time])

dnl Build up the directory we will use to install certs
TLSDATE_CA_ROOTS="${sysconfdir}/$PACKAGE_NAME/ca-roots"
AC_SUBST([TLSDATE_CA_ROOTS])

dnl Place we install our config file
TLSDATE_CONF_DIR="${sysconfdir}/$PACKAGE_NAME/"
AC_SUBST([TLSDATE_CONF_DIR])

dnl HTTPS User-agent
AC_ARG_WITH([https-user-agent],
	    [AS_HELP_STRING([--with-https-user-agent=AGENT],
	                    [a User-Agent value to send when running in HTTPS mode])],
	    [],
	    [with_https_user_agent="TLSDate/1.0"])
AC_DEFINE_UNQUOTED([HTTPS_USER_AGENT],
	           ["${with_https_user_agent}"],
		   [User-Agent value to send when running as an HTTPS client])

dnl check for PolarSSL
OPT_POLARSSL=yes

AC_MSG_CHECKING([PolarSSL])
AC_ARG_WITH([polarssl],
            [AS_HELP_STRING([--with-polarssl=DIR],
                            [where to look for PolarSSL, DIR points to the installation root])])

AS_CASE([$with_polarssl],
        [""|yes|no], [POLARSSL_DIR=""],
        [*], [POLARSSL_DIR=$with_polarssl])
OPT_POLARSSL=$with_polarssl

SSL_FLAGS=""
SSL_LDFLAGS=""
SSL_LIBS="-lssl -lcrypto"

AS_IF([test "x${OPT_POLARSSL}" != "xno"], [
  AS_IF([test -z "${POLARSSL_DIR}"], [
    dnl check for lib first without setting any new path
    AC_CHECK_LIB(polarssl, ssl_init,
    dnl libpolarssl found, set the variable
     [
       AC_DEFINE(USE_POLARSSL, 1, [if PolarSSL is enabled])
       AC_SUBST(USE_POLARSSL, [1])
       POLARSSL_ENABLED=1
       USE_POLARSSL="yes"
     ])
  ])

  addld=""
  addlib=""
  addcflags=""
  polarssllib=""

  AS_IF([test "x${USE_POLARSSL}" != "xyes"], [
    dnl add the path and test again
    addld=-L${POLARSSL_DIR}/lib$libsuff
    addcflags=-I${POLARSSL_DIR}/include
    polarssllib=${POLARSSL_DIR}/lib$libsuff

    LDFLAGS="$LDFLAGS $addld"
    AS_IF([test "$addcflags" != "-I/usr/include"], [
        AX_APPEND_COMPILE_FLAGS(["$addcflags"])
       CPPFLAGS="$CPPFLAGS $addcflags"
    ])

    AC_CHECK_LIB(polarssl, ssl_init,
     [
       AC_DEFINE(USE_POLARSSL, 1, [if PolarSSL is enabled])
       AC_SUBST(USE_POLARSSL, [1])
       POLARSSL_ENABLED=1
       USE_POLARSSL="yes"
       SSL_CFLAGS=$addcflags
       SSL_LDFLAGS=$addld
     ])
  ])

  AS_IF([test "x${USE_POLARSSL}" = "xyes"], [
    AC_MSG_NOTICE([detected PolarSSL])
    SSL_LIBS="-lpolarssl"

    AS_IF([test -n "$polarssllib"], [
      dnl when shared libs were found in a path that the run-time
      dnl linker doesn't search through, we need to add it to
      dnl LD_LIBRARY_PATH to prevent further configure tests to fail
      dnl due to this
      AS_IF([test "x$cross_compiling" != "xyes"], [
        LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$polarssllib"
        export LD_LIBRARY_PATH
        AC_MSG_NOTICE([Added $polarssllib to LD_LIBRARY_PATH])
      ])
    ])
  ])
])
AC_SUBST(SSL_CFLAGS)
AC_SUBST(SSL_LDFLAGS)
AC_SUBST(SSL_LIBS)
AM_CONDITIONAL(POLARSSL, test "x${USE_POLARSSL}" = "xyes")

dnl Required headers
AS_IF([test "x${USE_POLARSSL}" != "xyes"], [
  dnl First check to see if openssl is installed
  AC_CHECK_HEADERS([openssl/ssl.h], ,[AC_MSG_ERROR([OpenSSL is not installed, openssl/sslh is missing])])
  AC_CHECK_HEADERS([openssl/bio.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
  AC_CHECK_HEADERS([openssl/err.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
  AC_CHECK_HEADERS([openssl/evp.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
])

AC_CHECK_HEADERS([arpa/inet.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([getopt.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([grp.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([pwd.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([stdint.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([stdio.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([stdlib.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([sys/mman.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([sys/socket.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([sys/time.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([sys/types.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([sys/wait.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([time.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
AC_CHECK_HEADERS([unistd.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])

AC_CHECK_HEADERS([linux/rtc.h])
AC_CHECK_TYPES([struct rtc_time], [], [], [
#ifdef HAVE_LINUX_RTC_H
#include <linux/rtc.h>
#endif
])

AC_CHECK_FUNCS([strnlen])
AM_CONDITIONAL(HAVE_STRNLEN, [test "x${ac_cv_func_strnlen}" = xyes])

AC_CHECK_FUNCS([setresuid])
AC_CHECK_FUNCS([gettimeofday])

AC_CHECK_FUNCS([fmemopen funopen])
AM_CONDITIONAL(HAVE_FMEMOPEN, [test "x${ac_cv_func_fmemopen}" = xyes])
AM_CONDITIONAL(HAVE_FUNOPEN, [test "x${ac_cv_func_funopen}" = xyes])

case "$host" in
  *-darwin*)
    dnl This is for Mac OS X (10.8.2)
    AC_CHECK_HEADERS([mach/clock.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
    AC_CHECK_HEADERS([mach/mach.h], ,[AC_MSG_ERROR([Required headers missing; compilation will not succeed])])
    AC_ARG_WITH([unpriv-group],
                [AS_HELP_STRING([--with-unpriv-group=<group>],
                                [Group to drop privs to @<:@default: nogroup@:>@])])
    AS_CASE([$with_unpriv_group],
            [""|yes|no], [UNPRIV_GROUP="nobody"],
            [*], [UNPRIV_GROUP=$with_unpriv_group])
    AC_DEFINE_UNQUOTED([UNPRIV_GROUP], ["${UNPRIV_GROUP}"], [Unprivileged group])
    ;;
  *-freebsd*)
    dnl This is for FreeBSD
    dnl clock_gettime is either part of libc or unavailable.
    AC_CHECK_FUNC([clock_gettime], ,
                  [AC_MSG_ERROR([Your system lacks clock_gettime])])
    dnl If the autoconf goo picks up a compiler that runs in pre-POSIX mode,
    dnl the fmemopen prototype is hidden causing the unit tests to segfault.
    dnl This can happen if gcc is a symlink to gcc46 and is preferred to clang.
    AC_CHECK_FUNC([fmemopen], ,
                  [AC_MSG_ERROR([Missing fmemopen, unit tests are likely to segfault. Try CC=clang.])])
    AC_ARG_WITH([unpriv-group],
                [AS_HELP_STRING([--with-unpriv-group=<group>],
                [Group to drop privs to @<:@default: nobody@:>@])])
    AS_CASE([$with_unpriv_group],
            [""|yes|no], [UNPRIV_GROUP="nobody"],
            [*], [UNPRIV_GROUP=$with_unpriv_group])
    AC_DEFINE_UNQUOTED([UNPRIV_GROUP], ["${UNPRIV_GROUP}"], [Unprivileged group])
    ;;
  *kfreebsd*-gnu*)
    dnl This is for Debian GNU/kFreeBSD
    dnl clock_gettime is either part of libc or unavailable.
    dnl Check for clock_gettime.  Some systems put it into -lc, while
    dnl others use -lrt.  Try the first and fallback to the latter.
    RT_LIB=
    AC_CHECK_FUNC([clock_gettime], [:],
                  [AC_CHECK_LIB([rt], [clock_gettime], [RT_LIB="-lrt"],
                  [AC_MSG_ERROR([Your system lacks clock_gettime])])])
    AC_SUBST(RT_LIB)
    dnl If the autoconf goo picks up a compiler that runs in pre-POSIX mode,
    dnl the fmemopen prototype is hidden causing the unit tests to segfault.
    dnl This can happen if gcc is a symlink to gcc46 and is preferred to clang.
    AC_CHECK_FUNC([fmemopen], ,
                  [AC_MSG_ERROR([Missing fmemopen, unit tests are likely to segfault. Try CC=clang.])])
    AC_ARG_WITH([unpriv-group],
                [AS_HELP_STRING([--with-unpriv-group=<group>],
                [Group to drop privs to @<:@default: nobody@:>@])])
    AS_CASE([$with_unpriv_group],
            [""|yes|no], [UNPRIV_GROUP="nobody"],
            [*], [UNPRIV_GROUP=$with_unpriv_group])
    AC_DEFINE_UNQUOTED([UNPRIV_GROUP], ["${UNPRIV_GROUP}"], [Unprivileged group])
    ;;
  *-netbsd*)
    dnl This is for NetBSD
    dnl clock_gettime is either part of libc or unavailable.
    AC_CHECK_FUNC([clock_gettime], ,
                  [AC_MSG_ERROR([Your system lacks clock_gettime])])
    if test "x${ac_cv_func_fmemopen}" != xyes; then
        if test "x${ac_cv_func_funopen}" != xyes; then
            AC_MSG_ERROR([We need fmemopen or funopen for unit tests.])
        fi
    fi
    AC_ARG_WITH([unpriv-group],
                [AS_HELP_STRING([--with-unpriv-group=<group>],
                [Group to drop privs to @<:@default: nobody@:>@])])
    AS_CASE([$with_unpriv_group],
            [""|yes|no], [UNPRIV_GROUP="nobody"],
            [*], [UNPRIV_GROUP=$with_unpriv_group])
    AC_DEFINE_UNQUOTED([UNPRIV_GROUP], ["${UNPRIV_GROUP}"], [Unprivileged group])
    ;;
  *-openbsd*)
    dnl This is for OpenBSD
    dnl clock_gettime is either part of libc or unavailable.
    AC_CHECK_FUNC([clock_gettime], ,
                  [AC_MSG_ERROR([Your system lacks clock_gettime])])
    dnl If the autoconf goo picks up a compiler that runs in pre-POSIX mode,
    dnl the fmemopen prototype is hidden causing the unit tests to segfault.
    dnl This can happen if gcc is a symlink to gcc46 and is preferred to clang.
    AC_CHECK_FUNC([fmemopen], ,
                  [AC_MSG_WARN([Missing fmemopen, unit tests are likely to segfault. Try CC=clang.])])
    AC_ARG_WITH([unpriv-group],
                [AS_HELP_STRING([--with-unpriv-group=<group>],
                [Group to drop privs to @<:@default: nobody@:>@])])
    AS_CASE([$with_unpriv_group],
            [""|yes|no], [UNPRIV_GROUP="nobody"],
            [*], [UNPRIV_GROUP=$with_unpriv_group])
    AC_DEFINE_UNQUOTED([UNPRIV_GROUP], ["${UNPRIV_GROUP}"], [Unprivileged group])
    ;;
  *dragonfly*)
    dnl This is for DragonFly BSD
    dnl clock_gettime is either part of libc or unavailable.
    AC_CHECK_FUNC([clock_gettime], ,
                  [AC_MSG_ERROR([Your system lacks clock_gettime])])
    dnl If the autoconf goo picks up a compiler that runs in pre-POSIX mode,
    dnl the fmemopen prototype is hidden causing the unit tests to segfault.
    dnl This can happen if gcc is a symlink to gcc46 and is preferred to clang.
    AC_CHECK_FUNC([fmemopen], ,
                  [AC_MSG_WARN([Missing fmemopen, unit tests are likely to segfault. Try CC=clang.])])
    AC_ARG_WITH([unpriv-group],
                [AS_HELP_STRING([--with-unpriv-group=<group>],
                [Group to drop privs to @<:@default: nobody@:>@])])
    AS_CASE([$with_unpriv_group],
            [""|yes|no], [UNPRIV_GROUP="nobody"],
            [*], [UNPRIV_GROUP=$with_unpriv_group])
    AC_DEFINE_UNQUOTED([UNPRIV_GROUP], ["${UNPRIV_GROUP}"], [Unprivileged group])
    ;;
  *-bsd*)
    dnl This is a generic catch for BSD variants
    dnl This likely needs to be tuned to catch all
    dnl clock_gettime is either part of libc or unavailable.
    AC_CHECK_FUNC([clock_gettime], ,
                  [AC_MSG_ERROR([Your system lacks clock_gettime])])
    dnl If the autoconf goo picks up a compiler that runs in pre-POSIX mode,
    dnl the fmemopen prototype is hidden causing the unit tests to segfault.
    dnl This can happen if gcc is a symlink to gcc46 and is preferred to clang.
    AC_CHECK_FUNC([fmemopen], ,
                  [AC_MSG_ERROR([Missing fmemopen, unit tests are likely to segfault. Try CC=clang.])])
    AC_ARG_WITH([unpriv-group],
                [AS_HELP_STRING([--with-unpriv-group=<group>],
                [Group to drop privs to @<:@default: nobody@:>@])])
    AS_CASE([$with_unpriv_group],
            [""|yes|no], [UNPRIV_GROUP="nobody"],
            [*], [UNPRIV_GROUP=$with_unpriv_group])
    AC_DEFINE_UNQUOTED([UNPRIV_GROUP], ["${UNPRIV_GROUP}"], [Unprivileged group])
    ;;
  *-linux*)
    dnl This is for GNU/Linux
    dnl Check for clock_gettime.  Some systems put it into -lc, while
    dnl others use -lrt.  Try the first and fallback to the latter.
    RT_LIB=
    AC_CHECK_FUNC([clock_gettime], [:],
                  [AC_CHECK_LIB([rt], [clock_gettime], [RT_LIB="-lrt"],
                  [AC_MSG_ERROR([Your system lacks clock_gettime])])])
    AC_SUBST(RT_LIB)
    AC_ARG_WITH([unpriv-group],
                [AS_HELP_STRING([--with-unpriv-group=<group>],
                [Group to drop privs to @<:@default: nogroup@:>@])])
    AS_CASE([$with_unpriv_group],
            [""|yes|no], [UNPRIV_GROUP="nogroup"],
            [*], [UNPRIV_GROUP=$with_unpriv_group])
    AC_DEFINE_UNQUOTED([UNPRIV_GROUP], ["${UNPRIV_GROUP}"], [Unprivileged group])
    case "$host" in
      *-linux-androideabi)
        dnl This is for Android NDK as it is a special case of linux
        AC_DEFINE(HAVE_ANDROID,1, [Defined if we are to build for an Android system])
        AC_SUBST(HAVE_ANDROID, [1])
        HAVE_ANDROID="yes"
        ;;
    esac
    ;;
  *-gnu0.*)
    dnl This is a generic catch for GNU/Hurd variants
    dnl Check for clock_gettime.  Some systems put it into -lc, while
    dnl others use -lrt.  Try the first and fallback to the latter.
    RT_LIB=
    AC_CHECK_FUNC([clock_gettime], [:],
                  [AC_CHECK_LIB([rt], [clock_gettime], [RT_LIB="-lrt"],
                  [AC_MSG_ERROR([Your system lacks clock_gettime])])])
    AC_SUBST(RT_LIB)
    dnl If the autoconf goo picks up a compiler that runs in pre-POSIX mode,
    dnl the fmemopen prototype is hidden causing the unit tests to segfault.
    dnl This can happen if gcc is a symlink to gcc46 and is preferred to clang.
    AC_CHECK_FUNC([fmemopen], ,
                  [AC_MSG_ERROR([Missing fmemopen, unit tests are likely to segfault. Try CC=clang.])])
    AC_ARG_WITH([unpriv-group],
                [AS_HELP_STRING([--with-unpriv-group=<group>],
                [Group to drop privs to @<:@default: nobody@:>@])])
    AS_CASE([$with_unpriv_group],
            [""|yes|no], [UNPRIV_GROUP="nogroup"],
            [*], [UNPRIV_GROUP=$with_unpriv_group])
    AC_DEFINE_UNQUOTED([UNPRIV_GROUP], ["${UNPRIV_GROUP}"], [Unprivileged group])
    ;;
  *-cygwin*)
    dnl This is for Cygwin
    dnl Check for clock_gettime.  Some systems put it into -lc, while
    dnl others use -lrt.  Try the first and fallback to the latter.
    RT_LIB=
    AC_CHECK_FUNC([clock_gettime], [:],
                  [AC_CHECK_LIB([rt], [clock_gettime], [RT_LIB="-lrt"],
                  [AC_MSG_ERROR([Your system lacks clock_gettime])])])
    AC_SUBST(RT_LIB)
    AC_ARG_WITH([unpriv-group],
                [AS_HELP_STRING([--with-unpriv-group=<group>],
                [Group to drop privs to @<:@default: nogroup@:>@])])
    AS_CASE([$with_unpriv_group],
            [""|yes|no], [UNPRIV_GROUP="nogroup"],
            [*], [UNPRIV_GROUP=$with_unpriv_group])
    AC_DEFINE_UNQUOTED([UNPRIV_GROUP], ["${UNPRIV_GROUP}"], [Unprivileged group])
    ;;
  *-mingw32*)
    dnl This is for MINGW32_NT
    dnl Check for clock_gettime.  Some systems put it into -lc, while
    dnl others use -lrt.  Try the first and fallback to the latter.
    RT_LIB=
    AC_CHECK_FUNC([clock_gettime], [:],
                  [AC_CHECK_LIB([rt], [clock_gettime], [RT_LIB="-lrt"],
                  [AC_MSG_ERROR([Your system lacks clock_gettime])])])
    AC_SUBST(RT_LIB)
    AC_ARG_WITH([unpriv-group],
                [AS_HELP_STRING([--with-unpriv-group=<group>],
                [Group to drop privs to @<:@default: nogroup@:>@])])
    AS_CASE([$with_unpriv_group],
            [""|yes|no], [UNPRIV_GROUP="nogroup"],
            [*], [UNPRIV_GROUP=$with_unpriv_group])
    AC_DEFINE_UNQUOTED([UNPRIV_GROUP], ["${UNPRIV_GROUP}"], [Unprivileged group])
    ;;
  *-beos*|*-haiku*)
    dnl This is for BeOS and Haiku; we probably only support Haiku with gcc4
    dnl Check for clock_gettime.  Some systems put it into -lc, while
    dnl others use -lrt.  Try the first and fallback to the latter.
    dnl on Haiku we require the bsd library for strsep
    RT_LIB=
    AC_CHECK_FUNC([clock_gettime], [:],
                  [AC_CHECK_LIB([rt], [clock_gettime], [RT_LIB="-lrt"],
                  [AC_MSG_ERROR([Your system lacks clock_gettime])])])
    AC_SUBST(RT_LIB)
    AC_ARG_WITH([unpriv-group],
                [AS_HELP_STRING([--with-unpriv-group=<group>],
                [Group to drop privs to @<:@default: nogroup@:>@])])
    AS_CASE([$with_unpriv_group],
            [""|yes|no], [UNPRIV_GROUP="nogroup"],
            [*], [UNPRIV_GROUP=$with_unpriv_group])
    AC_DEFINE_UNQUOTED([UNPRIV_GROUP], ["${UNPRIV_GROUP}"], [Unprivileged group])
    LDFLAGS="$LDFLAGS -lbsd"
    CC="/boot/develop/abi/x86/gcc4/tools/gcc-4.6.3-haiku-121101/bin/gcc"
    ;;
esac

dnl Android conditional
AM_CONDITIONAL(HAVE_ANDROID, test "x${HAVE_ANDROID}" = "xyes")

AC_MSG_CHECKING([user/group to drop privs to])

AC_ARG_WITH([unpriv-user],
            [AS_HELP_STRING([--with-unpriv-user=<user>],
                            [User to drop privs to @<:@default: nobody@:>@])])
AS_CASE([$with_unpriv_user],
        [""|yes|no], [UNPRIV_USER="nobody"],
        [*], [UNPRIV_USER=$with_unpriv_user])
AC_DEFINE_UNQUOTED([UNPRIV_USER], ["${UNPRIV_USER}"], [Unprivileged user])
AC_MSG_RESULT(${UNPRIV_USER}:${UNPRIV_GROUP})

AC_ARG_ENABLE([dbus],
	      [AS_HELP_STRING([--disable-dbus],
			      [Disable automatically dbus support])])
AS_IF([test "x$enable_dbus" = xyes], [
    PKG_CHECK_MODULES([DBUS], [dbus-1], [
	    AC_DEFINE([HAVE_DBUS], [1], [Enable dbus support])
	    AC_MSG_CHECKING([user/group to use for dbus])
	    AC_ARG_WITH([dbus-user],
			[AS_HELP_STRING([--with-dbus-user=<user>],
					[User to send dbus signals from @<:@default: nobody@:>@])])
	    AS_CASE([$with_dbus_user],
		    [""|yes|no], [DBUS_USER="nobody"],
		    [*], [DBUS_USER=$with_dbus_user])
	    AC_ARG_WITH([dbus-group],
			[AS_HELP_STRING([--with-dbus-group=<group>],
					[Group to send dbus signals from @<:@default: nogroup@:>@])])
	    AS_CASE([$with_dbus_group],
		    [""|yes|no], [DBUS_GROUP="nogroup"],
		    [*], [DBUS_GROUP=$with_dbus_group])
	    AC_MSG_RESULT(${DBUS_USER}:${DBUS_GROUP})
	    AC_DEFINE_UNQUOTED([DBUS_USER], ["${DBUS_USER}"], [DBus user])
	    AC_DEFINE_UNQUOTED([DBUS_GROUP], ["${DBUS_GROUP}"], [DBus group])
	], [
	    AS_IF([test "x$enable_dbus" = xyes],
		  [AC_MSG_ERROR([dbus requested but not found])])
	])
    ])
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

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

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
