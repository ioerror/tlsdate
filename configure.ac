AC_INIT([tlsdate], [0.1], [jacob at appelbaum.net])
AM_INIT_AUTOMAKE([tlsdate], [0.1])
AM_CONFIG_HEADER([tlsdate-config.h])
AC_CANONICAL_HOST
AH_TOP([#define _GNU_SOURCE  1])

# Debug and hardening flags all in one shot
CFLAGS=" -g -O1 -Wall -fno-strict-aliasing "
CFLAGS+=" -D_FORTIFY_SOURCE=2 -fstack-protector-all "
CFLAGS+=" -fwrapv -fPIE -Wstack-protector "
CFLAGS+=" --param ssp-buffer-size=1 "
LDFLAGS=" -pie -z relro -z now "

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

AC_CHECK_FUNCS([setresuid])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
