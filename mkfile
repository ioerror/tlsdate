</$objtype/mkfile

#src/tlsdate: src/tlsdate.c
#	$CC -I. $prereq

src/tlsdatehelper:  config.h
	$CC  -I. ./src/util-plan9.c ./src/proxy-bio-plan9.c ./src/tlsdate-helper-plan9.c /$objtype/lib/ape/libssl.a  /$objtype/lib/ape/libcrypto.a

config.h:
	touch config.h

CC=pcc -DHAVE_TIME_H -D_PLAN9_SOURCE -D_REENTRANT_SOURCE -D_BSD_EXTENSION -D_SUSV2_SOURCE -D_POSIX_SOURCE
