AC_INIT([tlsdate], [0.0.1], [jacob at appelbaum.net])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER([config/tlsdate-config.h])
AM_INIT_AUTOMAKE([tlsdate], [0.0.1])
AC_CANONICAL_HOST
AH_TOP([#define _GNU_SOURCE  1])

# Debug and hardening flags all in one shot
CFLAGS=" -g -O1 -Wall -fno-strict-aliasing "
CFLAGS+=" -D_FORTIFY_SOURCE=2 -fstack-protector-all "
CFLAGS+=" -fwrapv -fPIE -Wstack-protector "
CFLAGS+=" --param ssp-buffer-size=1 "
LDFLAGS=" -pie -z relro -z now "

dnl Here we should build a small program to fetch the build system time in a portable
dnl manner. We have no Win32 users, we can fix this if we ever find one that
dnl cares.
COMPILE_DATE=`date +%s`
AC_SUBST(COMPILE_DATE)
AC_DEFINE_UNQUOTED([RECENT_COMPILE_DATE],
                   [(uint32_t) ${COMPILE_DATE}],
                   [Time in seconds since the Disco epoch at build time])

# Required headers
AC_CHECK_HEADERS(
  stdio.h  \
  stdint.h \
  stdlib.h \
  unistd.h \
  getopt.h \
  sys/time.h \
  sys/types.h \
  sys/wait.h \
  sys/mman.h \
  time.h \
  pwd.h \
  grp.h \
  arpa/inet.h \
  openssl/bio.h \
  openssl/ssl.h \
  openssl/err.h \
  openssl/evp.h 
, , AC_MSG_ERROR([Required headers missing; compilation will not succeed]))

AC_CHECK_FUNCS([setresuid gettimeofday])

AC_OUTPUT(Makefile src/Makefile)
